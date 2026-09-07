// Unlit looping state pictograms. Data.xy are activity flags, not counts.
struct Shapes {
 float stroke(float2 p,float2 a,float2 b,float w) {
  float2 d=b-a; return 1-smoothstep(w,w+.012,length(p-a-d*saturate(dot(p-a,d)/dot(d,d))));
 }
 float cleaver(float2 p,float expand) {
  // User's pointed chef/meat knife: straight spine at -X, curved edge at +X.
  // +X is the leading side of the clockwise downward stroke (before mirror).
  float2 v[5]={float2(-.07,-.62),float2(.035,-.46),
   float2(.11,-.23),float2(.13,.025),float2(-.07,.025)};
  float side=-100;
  for(int i=0;i<5;i++) {
   float2 a=v[i],d=v[(i+1)%5]-a;
   side=max(side,(d.y*(p.x-a.x)-d.x*(p.y-a.y))/length(d));
  }
  float blade=1-smoothstep(expand,expand+.012,side);
  float handle=stroke(p,float2(-.02,.07),float2(-.035,.31),.041+expand);
  float bolster=stroke(p,float2(-.085,.045),float2(.09,.045),.019+expand);
  return max(blade,max(handle,bolster));
 }
};
Shapes s;
float2 p=(UV-.5)*float2(4,2);
float mask=0; float3 color=float3(.65,.72,.8);
if(Data.z>.5) {
 mask=max(s.stroke(p,float2(-.28,-.28),float2(.28,.28),.07),s.stroke(p,float2(-.28,.28),float2(.28,-.28),.07));
 color=float3(1,.35,.01);
} else if(Data.x+Data.y<.5) {
 float bob=.06*sin(T*2); p.y+=bob;
 mask=max(s.stroke(p,float2(-.45,.15),float2(-.35,.38),.035),s.stroke(p,float2(-.35,.38),float2(.35,.38),.035));
 mask=max(mask,s.stroke(p,float2(.35,.38),float2(.45,.15),.035));
 mask=max(mask,s.stroke(p,float2(0,-.5),float2(0,.02),.045));
 mask=max(mask,s.stroke(p,float2(-.19,-.16),float2(0,.03),.045));
 mask=max(mask,s.stroke(p,float2(.19,-.16),float2(0,.03),.045));
} else {
 bool left=p.x<0; bool active=(left?Data.x:Data.y)>.5;
 p.x+=left?1:-1;
 color=left?float3(.015,.25,1):float3(1,.015,.008);
 if(active) {
  float2 q=p;
  if(left) {
   float a=.14*sin(T*3); q=mul(float2x2(cos(a),-sin(a),sin(a),cos(a)),q);
   float ellipse=length(q/float2(.30*(1+.28*q.y),.4));
   mask=1-smoothstep(.07,.11,abs(ellipse-1));
   if(frac(T*.35)>.35) {
    mask=max(mask,s.stroke(q,float2(-.12,-.15),float2(.035,-.035),.02));
    mask=max(mask,s.stroke(q,float2(.035,-.035),float2(-.045,.07),.02));
    mask=max(mask,s.stroke(q,float2(-.045,.07),float2(.1,.20),.02));
   }
  } else {
   float phase=frac(T*.8);
   // Fast edge-first cut, brief crossed hold, slower recovery.
   float swing=smoothstep(.12,.34,phase)*(1-smoothstep(.48,.98,phase));
   float front=0,frontBorder=0,back=0;
   // Left rotates clockwise into right/down; right is its reflected counterpart.
   // Both blades' convex cutting edges lead, never the straight spines.
   for(int hand=0;hand<2;hand++) {
    float direction=hand==0?1:-1;
    float2 pivot=float2(-direction*.30,.22);
    float a=lerp(-.50,1.10,swing);
    float2 local=p-pivot; local.x*=direction;
    local=mul(float2x2(cos(a),sin(a),-sin(a),cos(a)),local);
    local.y+=.23;
    float shape=s.cleaver(local,0);
    if(hand==0) back=shape;
    else {front=shape; frontBorder=s.cleaver(local,.025);}
   }
   // A narrow transparent separation keeps the rear blade readable at the X.
   mask=max(back*(1-frontBorder),front);
   color*=lerp(.66,1,front);
  }
 }
}
// Modest emission keeps the color saturated instead of blooming to white.
return float4(color*.85,saturate(mask));
