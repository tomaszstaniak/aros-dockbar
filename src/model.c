#include "model.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
static int equal(const char *a,const char *b) {
 while(*a && *b){if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++))return 0;}
 return *a==*b;
}
int dock_normalize_path(const char *path,char *out,size_t capacity) {
 if(!path || !out || !capacity)return 0;
 size_t n=strlen(path);
 if(n>5 && equal(path+n-5,".info"))n-=5;
 if(!n || n>=capacity || equal(path,".info"))return 0;
 for(size_t i=0;i<n;i++)if((unsigned char)path[i]<32)return 0;
 memcpy(out,path,n);out[n]=0;return 1;
}
static int *container_count(struct DockModel *m,int container) {
 if(!m || container<0 || container>DOCK_MAX_GROUPS)return NULL;
 int *count;
 if(container==0)count=&m->count;
 else {if(!m->groups[container-1].used)return NULL;count=&m->groups[container-1].count;}
 return *count>=0 && *count<=DOCK_MAX_ITEMS?count:NULL;
}
struct DockItem *dock_items(struct DockModel *m, int container, int *count) {
  int *n = container_count(m, container);
  if (count)
    *count = n ? *n : 0;
  if (!n)
    return NULL;
  return container ? m->groups[container - 1].items : m->items;
}
static int duplicate(const struct DockItem *items, int count,
                     const struct DockItem *item, int except) {
  if (item->kind == DOCK_APPLET) {
    for (int i = 0; i < count; i++)
      if (i != except && items[i].kind == DOCK_APPLET &&
          !strcmp(items[i].path, item->path) &&
          !strcmp(items[i].dir, item->dir))
        return 1;
    return 0;
  }
  if (item->kind != DOCK_APP && item->kind != DOCK_DRAWER)
    return 0;
  for (int i = 0; i < count; i++)
    if (i != except &&
        (items[i].kind == DOCK_APP || items[i].kind == DOCK_DRAWER) &&
        equal(items[i].path, item->path))
      return 1;
  return 0;
}
static void erase(struct DockItem *items, int *count, int index) {
  memmove(items + index, items + index + 1,
          (size_t)(*count - index - 1) * sizeof *items);
  --*count;
  memset(items + *count, 0, sizeof *items);
}
static void insert(struct DockItem *items, int *count, int before,
                   const struct DockItem *item) {
  memmove(items + before + 1, items + before,
          (size_t)(*count - before) * sizeof *items);
  items[before] = *item;
  ++*count;
}
int dock_insert(struct DockModel *m,int container,const struct DockItem *item,int before) {
 int *count=container_count(m,container);
 struct DockItem *items=dock_items(m,container,NULL);
 if(!count || !item || before<0 || before>*count)return -1;
 if(container && item->kind!=DOCK_APP)return -1;
 if(item->kind==DOCK_GROUP)return -1;  /* groups are created by dock_group_pair */
 if(item->kind==DOCK_COMMAND && !item->path[0])return -1;
 if(duplicate(items,*count,item,-1))return 0;
 if(*count==DOCK_MAX_ITEMS)return -1;
 insert(items,count,before,item);return 1;
}
int dock_add_kind(struct DockModel *m,int container,enum DockKind kind,const char *path,int before) {
 struct DockItem item={0};
 item.kind=kind;
 if(kind==DOCK_APP || kind==DOCK_DRAWER) {
  if(!dock_normalize_path(path,item.path,sizeof item.path))return -1;
  const char *label=item.path;
  for(const char *p=item.path;*p;p++)if((*p=='/' || *p==':') && p[1])label=p+1;
  snprintf(item.label,sizeof item.label,"%.*s",DOCK_LABEL-1,label);
 } else if(kind==DOCK_CLOCK) {
  snprintf(item.path,sizeof item.path,"%s",path && *path?path:"clock:date");
  strcpy(item.label,"Clock");
 } else if(kind!=DOCK_SEPARATOR && kind!=DOCK_SPACER)return -1;
 return dock_insert(m,container,&item,before);
}
int dock_add(struct DockModel *m,int container,const char *path,int before) {
 return dock_add_kind(m,container,DOCK_APP,path,before);
}
int dock_move(struct DockModel *m,int from,int index,int to,int before) {
 int *source_count=container_count(m,from),*target_count=container_count(m,to);
 struct DockItem *source=dock_items(m,from,NULL),*target=dock_items(m,to,NULL);
 if(!source_count || !target_count || index<0 || index>=*source_count || before<0 || before>*target_count)return -1;
 if(to && source[index].kind!=DOCK_APP)return -1;
 if(from==to && (before==index || before==index+1))return 0;
 if(duplicate(target,*target_count,&source[index],from==to?index:-1))return 0;
 if(from!=to && *target_count==DOCK_MAX_ITEMS)return -1;
 struct DockItem item=source[index];
 erase(source,source_count,index);
 if(from==to && before>index)--before;
 insert(target,target_count,before,&item);return 1;
}
int dock_group_pair(struct DockModel *m,int from,int index,int targetRootIndex) {
 int *source_count=container_count(m,from),*root_count=container_count(m,0);
 struct DockItem *source=dock_items(m,from,NULL);
 if(!source_count || !root_count || index<0 || index>=*source_count ||
  targetRootIndex<0 || targetRootIndex>=*root_count)return -1;
 if(from==0 && index==targetRootIndex)return 0;
 if(source[index].kind!=DOCK_APP || m->items[targetRootIndex].kind!=DOCK_APP)return -1;
 if(equal(source[index].path,m->items[targetRootIndex].path))return 0;
 int group=0;while(group<DOCK_MAX_GROUPS && m->groups[group].used)group++;
 if(group==DOCK_MAX_GROUPS)return -1;
 struct DockItem moving=source[index],target=m->items[targetRootIndex],item={0};
 item.kind=DOCK_GROUP;item.group=group+1;strcpy(item.label,"Group");
 struct DockGroup *created=&m->groups[group];memset(created,0,sizeof *created);
 created->used=1;created->count=2;created->items[0]=target;created->items[1]=moving;
 m->items[targetRootIndex]=item;
 erase(source,source_count,index);return 1;
}
int dock_remove(struct DockModel *m,int container,int index) {
 int *count=container_count(m,container);
 struct DockItem *items=dock_items(m,container,NULL);
 if(!count || index<0 || index>=*count)return -1;
 if(items[index].kind==DOCK_GROUP) {
  int group=items[index].group;
  if(container || group<1 || group>DOCK_MAX_GROUPS || !m->groups[group-1].used)return -1;
  memset(&m->groups[group-1],0,sizeof m->groups[0]);
 }
 erase(items,count,index);return 1;
}
int dock_pin(struct DockModel *m,const char *path) {
 if(!container_count(m,0))return -1;
 /* Preserve the legacy insertion point before the first non-application. */
 int at=0;while(at<m->count && m->items[at].kind==DOCK_APP)at++;
 return dock_add(m,0,path,at);
}
int dock_save_pins(const struct DockModel *m,const char *file) {
 char temp[DOCK_PATH+8],previous[DOCK_PATH+8];
 if(!m || !file || strlen(file)>=DOCK_PATH)return 0;
 snprintf(temp,sizeof temp,"%s.new",file);snprintf(previous,sizeof previous,"%s.old",file);
 FILE *f=fopen(temp,"wb");if(!f)return 0;
 int ok=1;
 for(int i=0;i<m->count;i++)if(m->items[i].kind==DOCK_APP)
  if(fprintf(f,"%s\n",m->items[i].path)<0)ok=0;
 if(fclose(f))ok=0;
 if(!ok){remove(temp);return 0;}
 /* AmigaDOS Rename does not replace an existing destination. */
 FILE *old=fopen(file,"rb");
 if(!old && errno!=ENOENT){remove(temp);return 0;}
 if(old){
  while(fgetc(old)!=EOF){}int readable=!ferror(old);if(fclose(old))readable=0;
  if(!readable){remove(temp);return 0;}
  if(remove(previous) && errno!=ENOENT){remove(temp);return 0;}
  if(rename(file,previous)){remove(temp);return 0;}
 }
 if(rename(temp,file)){if(old)rename(previous,file);return 0;}
 if(old)remove(previous);
 return 1;
}
int dock_load_pins(struct DockModel *m,const char *file) {
 if(!m || !file || strlen(file)>=DOCK_PATH)return -1;
 FILE *f=fopen(file,"rb");int recovered=0;
 if(!f){
  if(errno!=ENOENT)return -1;
  char previous[DOCK_PATH+8];snprintf(previous,sizeof previous,"%s.old",file);
  f=fopen(previous,"rb");if(!f)return errno==ENOENT?0:-1;
  recovered=1; /* Preserve an interrupted replacement until explicitly repaired. */
 }
 char line[DOCK_PATH+2];int ok=!recovered;
 while(fgets(line,sizeof line,f)) {
  size_t n=strlen(line);
  if(n && line[n-1]!='\n' && !feof(f)){int ch;while((ch=fgetc(f))!=EOF && ch!='\n'){}ok=0;continue;}
  while(n && (line[n-1]=='\n' || line[n-1]=='\r'))line[--n]=0;
  if(n && dock_pin(m,line)<0)ok=0;
 }
 if(ferror(f))ok=0;
 fclose(f);return ok?1:-1;
}
int dock_item_width(const struct DockItem *i,int size){return i->kind==DOCK_SEPARATOR?12:i->kind==DOCK_CLOCK?78:i->kind==DOCK_APPLET?120:size+10;}
int dock_width(const struct DockModel *m,int size){int w=28;for(int i=0;i<m->count;i++)w+=dock_item_width(&m->items[i],size);return w;}
int dock_item_x(const struct DockModel *m,int index,int size){int x=20;for(int i=0;i<index;i++)x+=dock_item_width(&m->items[i],size);return x;}
int dock_hit(const struct DockModel *m,int x,int y,int top,int size){
 if(y<top || y>=top+size)return -1;
 for(int i=0;i<m->count;i++){int left=dock_item_x(m,i,size),w=dock_item_width(&m->items[i],size);if(x>=left && x<left+w)return m->items[i].kind==DOCK_SEPARATOR?-1:i;}
 return -1;
}
