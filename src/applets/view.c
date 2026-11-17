#include "view.h"
#include "json.h"
#include "snapshot.h"
#include <stdlib.h>
#include <string.h>
static int field(const struct DockJson *j,int at,const char *key,char *out,size_t n,int required){
 int token=dock_json_get(j,at,key);
 return token<0?!required:dock_json_string(j,token,out,n);
}
static int leaf(const struct DockJson *j,int t,struct DockViewNode *n){
 char word[16];int v;
 if(t<0||j->token[t].type!=DJ_OBJECT)return 0;
 memset(n,0,sizeof *n);
 if(dock_json_get(j,t,"separator")>=0)n->type=DV_SEPARATOR;
 else if((v=dock_json_get(j,t,"spacer"))>=0){int size;if(!dock_json_int(j,v,&size)||size<1||size>200)return 0;n->type=DV_SPACER;n->size=size;}
 else if((v=dock_json_get(j,t,"icon"))>=0){
  int size=32;if(!dock_json_string(j,v,n->source,sizeof n->source))return 0;
  if((v=dock_json_get(j,t,"size"))>=0&&(!dock_json_int(j,v,&size)||size<8||size>128))return 0;
  n->type=DV_ICON;n->size=size;
 }else if((v=dock_json_get(j,t,"trend"))>=0){
  if(!dock_json_string(j,v,n->number,sizeof n->number)||!field(j,t,"text",n->source,sizeof n->source,0))return 0;
  n->type=DV_TREND;
 }else if(dock_json_get(j,t,"text")>=0||dock_json_get(j,t,"literal")>=0){
  if(!field(j,t,"text",n->source,sizeof n->source,0)||!field(j,t,"literal",n->literal,sizeof n->literal,0))return 0;
  n->type=DV_TEXT;
 }else return 0;
 if((v=dock_json_get(j,t,"style"))>=0){
  if(!dock_json_string(j,v,word,sizeof word))return 0;
  if(!strcmp(word,"muted"))n->style=DV_MUTED;else if(!strcmp(word,"big"))n->style=DV_BIG;else if(strcmp(word,"body"))return 0;
 }
 if((v=dock_json_get(j,t,"align"))>=0){
  if(!dock_json_string(j,v,word,sizeof word))return 0;
  if(!strcmp(word,"right"))n->right=1;else if(strcmp(word,"left"))return 0;
 }
 return 1;
}
static int layout(const struct DockJson *j,int a,struct DockAppletView *v){
 if(j->token[a].count<1)return 0;
 for(int i=0;i<j->token[a].count;i++){
  int t=dock_json_at(j,a,i),row,section;struct DockViewNode *n;
  if(t<0||j->token[t].type!=DJ_OBJECT||v->layout_count>=DV_NODES)return 0;
  n=&v->layout[v->layout_count];
  row=dock_json_get(j,t,"row");section=dock_json_get(j,t,"list");
  if(row<0&&section<0){if(!leaf(j,t,n))return 0;v->layout_count++;continue;}
  if(row<0||j->token[row].type!=DJ_ARRAY||j->token[row].count<1||j->token[row].count>8)return 0;
  memset(n,0,sizeof *n);n->type=DV_ROW;
  if(section>=0){
   int max=5,m=dock_json_get(j,t,"max");
   if(!dock_json_string(j,section,n->source,sizeof n->source)||!n->source[0])return 0;
   if(m>=0&&(!dock_json_int(j,m,&max)||max<1||max>DV_LIST_MAX))return 0;
   n->type=DV_LIST;n->size=max;
  }
  n->count=j->token[row].count;v->layout_count++;
  for(int k=0;k<n->count;k++){
   if(v->layout_count>=DV_NODES||!leaf(j,dock_json_at(j,row,k),&v->layout[v->layout_count]))return 0;
   v->layout_count++;
  }
 }
 return 1;
}
int dock_applet_view_parse(const char *s,size_t n,struct DockAppletView *out){
 struct DockJson *j=malloc(sizeof *j);struct DockAppletView v={0};int ok=0,version;
 if(!j)return 0;
 if(!dock_json_parse(j,s,n)||!dock_json_int(j,dock_json_get(j,0,"schema"),&version)||version!=1)goto done;
 int a=dock_json_get(j,0,"expanded");
 if(a>=0&&j->token[a].type==DJ_ARRAY){if(!layout(j,a,&v))goto done;*out=v;ok=1;goto done;}
 if(!dock_json_is(j,dock_json_get(j,a,"component"),"cards"))goto done;
 if(!field(j,a,"title",v.title,sizeof v.title,1)||!field(j,a,"value",v.value,sizeof v.value,1)||
    !field(j,a,"subtitle",v.subtitle,sizeof v.subtitle,0)||!field(j,a,"range",v.range,sizeof v.range,0)||
    !field(j,a,"strip",v.strip,sizeof v.strip,0)||!field(j,a,"list",v.list,sizeof v.list,0)||
    !field(j,a,"stripTitle",v.strip_title,sizeof v.strip_title,0)||!field(j,a,"listTitle",v.list_title,sizeof v.list_title,0))goto done;
 int m=dock_json_get(j,a,"metrics");
 if(m>=0){if(j->token[m].type!=DJ_ARRAY||j->token[m].count>2)goto done;
  for(int i=0;i<j->token[m].count;i++){
   int t=dock_json_at(j,m,i);
   if(!field(j,t,"field",v.metric[i],sizeof v.metric[i],1)||!field(j,t,"label",v.metric_label[i],sizeof v.metric_label[i],1))goto done;
  }
 }
 *out=v;ok=1;
 done:free(j);return ok;
}
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
void dock_applet_anchor(int sw,int sh,int bar,int w,int h,int ax,int ay,int aw,int ah,int vertical,int *x,int *y){
 if(vertical){*x=ax+aw+8;if(*x+w>sw-8)*x=ax-w-8;*y=ay+ah/2-h/2;}
 else{*x=ax+aw/2-w/2;*y=ay-h-8;if(*y<bar+8)*y=ay+ah+8;}
 *x=clamp(*x,8,sw-w-8);*y=clamp(*y,bar+8,sh-h-8);
}

static uint64_t hash_text(uint64_t hash,const char *s){
 for(;*s;s++){hash^=(unsigned char)*s;hash*=1099511628211ULL;}
 hash^=0;return hash*1099511628211ULL;
}
uint64_t dock_applet_compact_signature(const struct DockAppletSnapshot *s,const char *primary){
 return hash_text(hash_text(14695981039346656037ULL,s->icon),dock_applet_field(s,primary));
}
uint64_t dock_applet_view_signature(const struct DockAppletSnapshot *s,const struct DockAppletView *v,int page){
 uint64_t hash=hash_text(14695981039346656037ULL,s->icon);
 if(v->layout_count){ /* a described card may show anything: everything counts */
  for(int i=0;i<s->field_count;i++)hash=hash_text(hash,s->fields[i].text);
  for(int i=0;i<s->row_count;i++){const struct DockAppletRow *r=&s->rows[i];
   hash=hash_text(hash,r->label);hash=hash_text(hash,r->value);hash=hash_text(hash,r->detail);
   hash=hash_text(hash,r->caption);hash=hash_text(hash,r->icon);hash=hash_text(hash,r->has_plot?(r->plot>0?"+":r->plot<0?"-":"0"):"");}
  return hash;
 }
 const char *keys[]={v->title,v->value,v->subtitle,v->range,v->metric[0],v->metric[1]};
 for(unsigned i=0;i<sizeof keys/sizeof *keys;i++)hash=hash_text(hash,dock_applet_field(s,keys[i]));
 for(int section=0;section<2;section++){
  const char *key=section?v->list:v->strip;int seen=0,start=section?0:page,limit=section?5:6;
  if(!key[0])continue;
  for(int i=0;i<s->row_count;i++)if(!strcmp(key,s->rows[i].section)){
   if(seen++<start)continue;
   if(seen>start+limit)break;
   const struct DockAppletRow *r=&s->rows[i];
   hash=hash_text(hash,r->caption[0]?r->caption:r->label);hash=hash_text(hash,r->value);
   hash=hash_text(hash,r->detail);hash=hash_text(hash,r->icon);
  }
 }
 return hash;
}
