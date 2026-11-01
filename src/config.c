#include "config.h"
#include "config_io.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int dock_cfg_text_valid(const char *text,size_t capacity) {
 size_t i;
 for(i=0;i<capacity && text[i];i++)if((unsigned char)text[i]<32 || (unsigned char)text[i]==127)return 0;
 return i<capacity;
}
static int equal(const char *a,const char *b) {
 while(*a && *b){if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++))return 0;}
 return *a==*b;
}
/* Version 1 knows APP..DRAWER with empty clock paths; version 2 adds spacers
 * and clock settings in the path field. */
int dock_cfg_item_valid(const struct DockItem *item,int child,int version) {
 int last=version>=7?DOCK_APPLET:version>=7?DOCK_APPLET:version>=6?DOCK_COMMAND:version>=2?DOCK_SPACER:DOCK_DRAWER;
 if(item->present<DOCK_PRESENT_DOCK || item->present>DOCK_PRESENT_ICON_TEXT)return 0;
 if(item->kind<DOCK_APP || (int)item->kind>last || (child && item->kind!=DOCK_APP) ||
  !dock_cfg_text_valid(item->path,sizeof item->path) || !dock_cfg_text_valid(item->label,sizeof item->label) ||
  !dock_cfg_text_valid(item->icon,sizeof item->icon) || !dock_cfg_text_valid(item->dir,sizeof item->dir) ||
  item->run<0 || item->run>(DOCK_RUN_SHELL|DOCK_RUN_SCRIPT))return 0;
 if(item->on_drop!=DOCK_DROP_ADD && (item->on_drop!=DOCK_DROP_OPEN || (item->kind!=DOCK_APP && item->kind!=DOCK_COMMAND)))return 0;
 if(item->open!=DOCK_OPEN_WANDERER && (item->open!=DOCK_OPEN_PANEL || item->kind!=DOCK_DRAWER))return 0;
 if(item->kind!=DOCK_COMMAND && item->kind!=DOCK_APPLET && (item->dir[0] || item->run))return 0;
 if(item->kind==DOCK_APPLET){
  if(item->group || item->run || !item->path[0] || !item->dir[0] || strlen(item->path)>=80 || strlen(item->dir)>=80)return 0;
  for(const char *p=item->path;*p;p++)if(!isalnum((unsigned char)*p) && *p!='.' && *p!='-' && *p!='_')return 0;
  for(const char *p=item->dir;*p;p++)if(!isalnum((unsigned char)*p) && *p!='.' && *p!='-' && *p!='_')return 0;
  return strcmp(item->path,".") && strcmp(item->path,"..") && strcmp(item->dir,".") && strcmp(item->dir,"..");
 }
 if(item->kind==DOCK_COMMAND)return !item->group && item->path[0]!=0;
 if(item->kind==DOCK_GROUP)return item->group>=1 && item->group<=DOCK_MAX_GROUPS && !item->path[0];
 if(item->group)return 0;
 if(item->kind==DOCK_APP || item->kind==DOCK_DRAWER)return item->path[0]!=0;
 if(version>=2 && item->kind==DOCK_CLOCK)return !item->path[0] || !strncmp(item->path,"clock:",6);
 return !item->path[0];
}
static int items_valid(const struct DockItem *items,int count,int child,int version) {
 if(count<0 || count>DOCK_MAX_ITEMS)return 0;
 for(int i=0;i<count;i++) {
  if(!dock_cfg_item_valid(items+i,child,version))return 0;
  if(items[i].kind!=DOCK_APP && items[i].kind!=DOCK_DRAWER)continue;
  for(int j=0;j<i;j++)if((items[j].kind==DOCK_APP || items[j].kind==DOCK_DRAWER) && equal(items[j].path,items[i].path))return 0;
 }
 return 1;
}
int dock_cfg_model_valid(const struct DockModel *model,int version) {
 int references[DOCK_MAX_GROUPS]={0};
 if(!model || !items_valid(model->items,model->count,0,version))return 0;
 for(int i=0;i<model->count;i++)if(model->items[i].kind==DOCK_GROUP)references[model->items[i].group-1]++;
 for(int i=0;i<DOCK_MAX_GROUPS;i++) {
  const struct DockGroup *group=model->groups+i;
  if(group->used!=0 && group->used!=1)return 0;
  if(references[i]!=group->used)return 0;
  if(group->used){if(!items_valid(group->items,group->count,1,version))return 0;}
  else if(group->count)return 0;
 }
 return 1;
}
void dock_clamp_position(struct DockPosition *position,int sw,int sh,int width,int height) {
 if(!position)return;
 /* Widen before subtracting so malformed/extreme saved coordinates cannot
  * overflow the bounds calculation. Oversized windows anchor at (0,20). */
 long long right=(long long)sw-(width>0?width:0);
 long long bottom=(long long)sh-(height>0?height:0);
 if(right<0)right=0;
 if(right>INT_MAX)right=INT_MAX;
 if(bottom<20)bottom=20;
 if(bottom>INT_MAX)bottom=INT_MAX;
 if(position->x<0)position->x=0;
 if(position->x>right)position->x=(int)right;
 if(position->y<20)position->y=20;
 if(position->y>bottom)position->y=(int)bottom;
}
static int model_valid(const struct DockModel *model,const struct DockPosition *position) {
 return position && (position->valid==0 || position->valid==1) && dock_cfg_model_valid(model,1);
}
int dock_cfg_number(const char *text,int minimum,int maximum,int *result) {
 char *end;long value;
 if(!text || !*text || (*text!='-' && (*text<'0' || *text>'9')))return 0;
 errno=0;value=strtol(text,&end,10);
 if(errno || *end || value<minimum || value>maximum)return 0;
 *result=(int)value;return 1;
}
/* Every record must end in a newline; a truncated last record is invalid.
 * Splitting explicitly preserves empty and space-prefixed fields losslessly. */
int dock_cfg_record(FILE *file,char *line,char **fields,int capacity) {
 if(!fgets(line,CONFIG_LINE,file))return 0;
 size_t length=strlen(line);
 if(!length || line[length-1]!='\n')return 0;
 line[--length]=0;
 int count=1;fields[0]=line;
 for(size_t i=0;i<length;i++) {
  if(line[i]=='\t') {
   if(count==capacity)return 0;
   line[i]=0;fields[count++]=line+i+1;
  } else if((unsigned char)line[i]<32 || (unsigned char)line[i]==127)return 0;
 }
 return count;
}
int dock_cfg_read_item(FILE *file,struct DockItem *item,int version) {
 char line[CONFIG_LINE],*fields[11];int kind,want=version>=8?11:version>=6?9:version>=4?7:6;
 item->present=DOCK_PRESENT_DOCK;item->dir[0]=0;item->run=0;item->on_drop=DOCK_DROP_ADD;item->open=DOCK_OPEN_WANDERER;
 if(dock_cfg_record(file,line,fields,11)!=want || strcmp(fields[0],"ITEM") ||
  (want>=9 && (strlen(fields[7])>=sizeof item->dir || !dock_cfg_number(fields[8],0,DOCK_RUN_SHELL|DOCK_RUN_SCRIPT,&item->run))) ||
  (want>=11 && (!dock_cfg_number(fields[9],DOCK_DROP_ADD,DOCK_DROP_OPEN,&item->on_drop) ||
               !dock_cfg_number(fields[10],DOCK_OPEN_WANDERER,DOCK_OPEN_PANEL,&item->open))) ||
  (want>=7 && !dock_cfg_number(fields[6],DOCK_PRESENT_DOCK,DOCK_PRESENT_ICON_TEXT,&item->present)) ||
  !dock_cfg_number(fields[1],DOCK_APP,version>=7?DOCK_APPLET:version>=6?DOCK_COMMAND:version>=2?DOCK_SPACER:DOCK_DRAWER,&kind) ||
  !dock_cfg_number(fields[2],0,DOCK_MAX_GROUPS,&item->group) ||
  strlen(fields[3])>=sizeof item->path || strlen(fields[4])>=sizeof item->label || strlen(fields[5])>=sizeof item->icon)return 0;
 item->kind=(enum DockKind)kind;
 strcpy(item->path,fields[3]);strcpy(item->label,fields[4]);strcpy(item->icon,fields[5]);
 if(want>=9)strcpy(item->dir,fields[7]);
 return 1;
}
static int read_config(FILE *file,struct DockModel *model,struct DockPosition *position) {
 char line[CONFIG_LINE],*fields[4];int count;
 if(dock_cfg_record(file,line,fields,4)!=2 || strcmp(fields[0],"DOCKBAR") || strcmp(fields[1],"1"))return 0;
 if(dock_cfg_record(file,line,fields,4)!=4 || strcmp(fields[0],"POSITION") ||
  !dock_cfg_number(fields[1],0,1,&position->valid) || !dock_cfg_number(fields[2],INT_MIN,INT_MAX,&position->x) ||
  !dock_cfg_number(fields[3],INT_MIN,INT_MAX,&position->y))return 0;
 if(dock_cfg_record(file,line,fields,4)!=2 || strcmp(fields[0],"ROOT") || !dock_cfg_number(fields[1],0,DOCK_MAX_ITEMS,&model->count))return 0;
 for(int i=0;i<model->count;i++)if(!dock_cfg_read_item(file,model->items+i,1))return 0;
 while((count=dock_cfg_record(file,line,fields,4))!=0) {
  if(count==1 && !strcmp(fields[0],"END"))return fgetc(file)==EOF && !ferror(file) && model_valid(model,position);
  int group;
  if(count!=3 || strcmp(fields[0],"GROUP") || !dock_cfg_number(fields[1],1,DOCK_MAX_GROUPS,&group) || model->groups[group-1].used)return 0;
  struct DockGroup *target=model->groups+group-1;
  if(!dock_cfg_number(fields[2],0,DOCK_MAX_ITEMS,&target->count))return 0;
  target->used=1;
  for(int i=0;i<target->count;i++)if(!dock_cfg_read_item(file,target->items+i,1))return 0;
 }
 return 0;
}
int dock_config_load(struct DockModel *model,struct DockPosition *position,const char *path) {
 if(!model || !position || !path || !*path || !dock_cfg_text_valid(path,DOCK_PATH))return -1;
 FILE *file=fopen(path,"rb");int recovered=0;
 if(!file) {
  if(errno!=ENOENT)return -1;
  char previous[DOCK_PATH+8];snprintf(previous,sizeof previous,"%s.old",path);
  file=fopen(previous,"rb");if(!file)return errno==ENOENT?0:-1;
  recovered=1;
 }
 struct DockModel *candidate=calloc(1,sizeof *candidate);
 struct DockPosition candidate_position={0};
 if(!candidate){fclose(file);return -1;}
 int valid=read_config(file,candidate,&candidate_position);
 if(fclose(file))valid=0;
 if(valid){*model=*candidate;*position=candidate_position;}
 free(candidate);return valid && !recovered?1:-1;
}
int dock_cfg_write_item(FILE *file,const struct DockItem *item) {
 return fprintf(file,"ITEM\t%d\t%d\t%s\t%s\t%s\n",(int)item->kind,item->group,item->path,item->label,item->icon)>=0;
}
int dock_cfg_write_item_v4(FILE *file,const struct DockItem *item) {
 return fprintf(file,"ITEM\t%d\t%d\t%s\t%s\t%s\t%d\t%s\t%d\t%d\t%d\n",(int)item->kind,item->group,item->path,item->label,item->icon,item->present,item->dir,item->run,item->on_drop,item->open)>=0;
}
static int write_config(FILE *file,const struct DockModel *model,const struct DockPosition *position) {
 if(fprintf(file,"DOCKBAR\t1\nPOSITION\t%d\t%d\t%d\nROOT\t%d\n",position->valid,position->x,position->y,model->count)<0)return 0;
 for(int i=0;i<model->count;i++)if(!dock_cfg_write_item(file,model->items+i))return 0;
 for(int i=0;i<DOCK_MAX_GROUPS;i++)if(model->groups[i].used) {
  const struct DockGroup *group=model->groups+i;
  if(fprintf(file,"GROUP\t%d\t%d\n",i+1,group->count)<0)return 0;
  for(int j=0;j<group->count;j++)if(!dock_cfg_write_item(file,group->items+j))return 0;
 }
 return fputs("END\n",file)>=0;
}
/* Validate the old primary too: a future version, malformed file, or a
 * directory that fopen accepts must never become collateral of a new save. */
static int primary_state(const char *path) {
 FILE *file=fopen(path,"rb");
 if(!file)return errno==ENOENT?0:-1;
 struct DockModel *existing=calloc(1,sizeof *existing);
 struct DockPosition position={0};
 if(!existing){fclose(file);return -1;}
 int valid=read_config(file,existing,&position);if(fclose(file))valid=0;
 free(existing);return valid?1:-1;
}
int dock_config_save(const struct DockModel *model,const struct DockPosition *position,const char *path) {
 char temporary[DOCK_PATH+8],previous[DOCK_PATH+8];
 if(!path || !*path || !dock_cfg_text_valid(path,DOCK_PATH) || !model_valid(model,position))return 0;
 snprintf(temporary,sizeof temporary,"%s.new",path);snprintf(previous,sizeof previous,"%s.old",path);
 int exists=primary_state(path);
 if(exists<0)return 0;
 if(!exists) {
  FILE *backup=fopen(previous,"rb");
  if(backup){fclose(backup);return 0;}
  if(errno!=ENOENT)return 0;
 }
 FILE *file=fopen(temporary,"wb");if(!file)return 0;
 int valid=write_config(file,model,position);if(fclose(file))valid=0;
 if(!valid){remove(temporary);return 0;}
 /* AmigaDOS Rename cannot replace an existing destination. The recoverable
  * .old bridges that gap; never discard it when the primary is absent. */
 if(exists) {
  if(remove(previous) && errno!=ENOENT){remove(temporary);return 0;}
  if(rename(path,previous)){remove(temporary);return 0;}
 }
 if(rename(temporary,path)) {
  if(exists)rename(previous,path);
  remove(temporary);return 0;
 }
 if(exists)remove(previous);
 return 1;
}
