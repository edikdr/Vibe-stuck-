package app.nebula.archive

/**
 * The inspector that runs inside the archived page.
 *
 * It never mutates the page: outlines, selection marks and labels are painted on one fixed canvas above the
 * document, so a site cannot be broken by being inspected, and nothing has to be restored afterwards.
 *
 * Picking is two-stage on purpose. The first tap marks an element and leaves the user in the page, the second
 * tap on the same element inspects it. That makes a multi-selection possible with the only input a phone has.
 */
internal const val INSPECTOR_SCRIPT = """
function(enabled){
  var key='__nebulaInspector';
  if(window[key]){window[key].setEnabled(enabled);return}
  var MAX_OUTLINES=1200;
  function slice(v){return Array.prototype.slice.call(v||[])}
  function esc(v){return (window.CSS&&CSS.escape)?CSS.escape(String(v)):String(v).replace(/[^a-z0-9_-]/gi,'\\${'$'}&')}
  function ui(el){return el.closest?el.closest('[data-nebula-ui]'):null}
  function textOf(el,limit){return (el.innerText||el.textContent||'').trim().replace(/\s+/g,' ').slice(0,limit)}
  function segment(node){
    var s=node.tagName.toLowerCase();
    if(node.id)return s+'#'+esc(node.id);
    var c=slice(node.classList).filter(function(v){return v&&v.indexOf('nebula-')!==0}).slice(0,3);
    if(c.length)s+='.'+c.map(esc).join('.');
    var p=node.parentElement;
    if(p){var same=slice(p.children).filter(function(x){return x.tagName===node.tagName});if(same.length>1)s+=':nth-of-type('+(same.indexOf(node)+1)+')'}
    return s;
  }
  function selector(el){var parts=[],node=el,count=0;while(node&&node.nodeType===1&&count<16){parts.unshift(segment(node));count++;if(node.id||node===document.body)break;node=node.parentElement}return parts.join(' > ')}
  function depth(el){var d=0,n=el;while(n&&n.parentElement){d++;n=n.parentElement}return d}
  function ref(el){var t=textOf(el,52);return {selector:selector(el),tag:el.tagName.toLowerCase(),label:segment(el)+(t?' · '+t:''),depth:depth(el)}}
  function semanticRoot(el){var chain=[],n=el;while(n&&n!==document.body){chain.unshift(n);n=n.parentElement}if(!chain.length)return el;var i=0;if(chain[0].id==='root'||chain[0].id==='app'||chain[0].id==='__next')i=1;if(chain[i]&&chain[i].children.length===1&&chain[i+1])i++;return chain[Math.min(i,chain.length-1)]}
  function hash(value){var h=0;for(var i=0;i<value.length;i++)h=(h*31+value.charCodeAt(i))>>>0;return h}
  function colorFor(el,alpha){
    var root=semanticRoot(el),hue=hash(selector(root))%360,local=Math.max(0,depth(el)-depth(root)),light=Math.max(48,70-Math.min(local,7)*3);
    return alpha===undefined?'hsl('+hue+' 88% '+light+'%)':'hsl('+hue+' 88% '+light+'% / '+alpha+')';
  }
  function selectable(el){
    if(!el||el.nodeType!==1||ui(el))return false;
    if(/^(script|style|meta|link|title|head|html|body|noscript|template)${'$'}/i.test(el.tagName))return false;
    var r=el.getBoundingClientRect();
    if(r.width<4||r.height<4)return false;
    var s=getComputedStyle(el);
    return s.display!=='none'&&s.visibility!=='hidden'&&Number(s.opacity||1)>0;
  }
  function visibleElements(){
    var out=[],nodes=document.body?document.body.querySelectorAll('*'):[],w=innerWidth,h=innerHeight;
    for(var i=0;i<nodes.length&&out.length<MAX_OUTLINES;i++){
      var el=nodes[i];
      if(!selectable(el))continue;
      var r=el.getBoundingClientRect();
      if(r.bottom<0||r.top>h||r.right<0||r.left>w)continue;
      out.push([el,r]);
    }
    return out;
  }
  function attributesOf(el){
    var out=[],list=el.attributes||[];
    for(var i=0;i<list.length&&out.length<24;i++){
      var item=list[i];
      if(item.name==='style'&&String(item.value).length>200)continue;
      out.push({name:item.name,value:String(item.value).slice(0,180)});
    }
    return out;
  }
  function styles(el){
    var s=getComputedStyle(el),r=el.getBoundingClientRect(),out=[];
    function always(name,value){out.push({name:name,value:String(value).slice(0,120)})}
    function put(name,value){
      if(!value)return;
      var v=String(value).trim();
      if(!v||v==='none'||v==='normal'||v==='auto'||v==='0px'||v==='rgba(0, 0, 0, 0)'||v==='transparent')return;
      out.push({name:name,value:v.slice(0,120)});
    }
    always('size',Math.round(r.width)+' × '+Math.round(r.height));
    always('display',s.display);
    if(s.position!=='static')always('position',s.position+' · t'+s.top+' l'+s.left);
    always('color',s.color);
    put('background',s.backgroundColor);
    put('background-image',s.backgroundImage);
    always('font',s.fontSize+'/'+s.lineHeight+' '+s.fontWeight+' '+String(s.fontFamily||'').split(',')[0].replace(/["']/g,''));
    put('padding',s.padding);
    put('margin',s.margin);
    if(s.borderWidth&&s.borderWidth!=='0px')put('border',s.borderWidth+' '+s.borderStyle+' '+s.borderColor);
    put('radius',s.borderRadius);
    if(s.display.indexOf('flex')>=0){always('flex',s.flexDirection+' · '+s.justifyContent+' · '+s.alignItems);put('gap',s.gap)}
    if(s.display.indexOf('grid')>=0){put('grid',s.gridTemplateColumns);put('gap',s.gap)}
    put('overflow',s.overflow==='visible'?'':s.overflow);
    put('z-index',s.zIndex);
    if(s.opacity&&s.opacity!=='1')always('opacity',s.opacity);
    put('transform',s.transform);
    if(s.boxSizing&&s.boxSizing!=='content-box')always('box-sizing',s.boxSizing);
    return out;
  }
  function detail(el,id){
    var r=el.getBoundingClientRect(),parents=[],p=el.parentElement;
    while(p&&parents.length<14){parents.push(ref(p));p=p.parentElement}
    var children=slice(el.children).slice(0,24).map(ref);
    var siblings=el.parentElement?slice(el.parentElement.children).filter(function(x){return x!==el}).slice(0,24).map(ref):[];
    return {
      id:id||0,selector:selector(el),elementId:el.id||'',classes:slice(el.classList),tag:el.tagName.toLowerCase(),
      text:textOf(el,320),outerHtml:String(el.outerHTML||'').slice(0,6000),depth:depth(el),childCount:el.children.length,
      width:r.width,height:r.height,left:r.left,top:r.top,
      attributes:attributesOf(el),styles:styles(el),parents:parents,children:children,siblings:siblings
    };
  }
  function summary(item){
    var el=item.el,r=el.getBoundingClientRect(),t=textOf(el,40);
    return {id:item.id,selector:selector(el),tag:el.tagName.toLowerCase(),label:segment(el)+(t?' · '+t:''),depth:depth(el),childCount:el.children.length,width:r.width,height:r.height};
  }
  function emitSelection(){NebulaInspector.selection(JSON.stringify(state.selection.map(summary)))}
  function emitDetails(elements){NebulaInspector.inspected(JSON.stringify(elements))}
  function indexOf(el){for(var i=0;i<state.selection.length;i++)if(state.selection[i].el===el)return i;return -1}
  function find(id){for(var i=0;i<state.selection.length;i++)if(state.selection[i].id===id)return state.selection[i];return null}
  function prune(){
    var before=state.selection.length;
    state.selection=state.selection.filter(function(item){return item.el.isConnected!==false});
    if(state.selection.length!==before)emitSelection();
  }
  function canvas(){
    if(state.canvas&&state.canvas.isConnected)return state.canvas;
    var c=document.createElement('canvas');
    c.setAttribute('data-nebula-ui','');
    c.style.cssText='position:fixed;inset:0;width:100vw;height:100vh;pointer-events:none;z-index:2147483646;display:none';
    document.documentElement.appendChild(c);
    state.canvas=c;
    return c;
  }
  function badge(ctx,x,y,text,color){
    var w=text.length>1?20:16;
    ctx.fillStyle=color;ctx.fillRect(x,y,w,16);
    ctx.fillStyle='#05070e';ctx.font='bold 11px monospace';ctx.textBaseline='middle';
    ctx.fillText(text,x+(w-ctx.measureText(text).width)/2,y+8);
  }
  function draw(){
    var c=canvas();
    if(!state.enabled&&!state.selection.length){c.style.display='none';return}
    c.style.display='block';
    var ratio=window.devicePixelRatio||1,w=Math.max(1,innerWidth),h=Math.max(1,innerHeight);
    if(c.width!==Math.round(w*ratio)||c.height!==Math.round(h*ratio)){c.width=Math.round(w*ratio);c.height=Math.round(h*ratio)}
    var ctx=c.getContext('2d');
    ctx.setTransform(ratio,0,0,ratio,0,0);
    ctx.clearRect(0,0,w,h);
    if(state.enabled){
      var items=visibleElements();
      ctx.lineWidth=1;
      for(var i=0;i<items.length;i++){
        var r=items[i][1];
        ctx.strokeStyle=colorFor(items[i][0],.45);
        ctx.strokeRect(r.left+.5,r.top+.5,Math.max(0,r.width-1),Math.max(0,r.height-1));
      }
    }
    for(var s=0;s<state.selection.length;s++){
      var item=state.selection[s];
      if(item.el.isConnected===false)continue;
      var sr=item.el.getBoundingClientRect();
      if(sr.bottom<0||sr.top>h||sr.width<1)continue;
      var color=colorFor(item.el);
      ctx.fillStyle=colorFor(item.el,.16);
      ctx.fillRect(sr.left,sr.top,sr.width,sr.height);
      ctx.strokeStyle=color;ctx.lineWidth=2;
      ctx.strokeRect(sr.left+1,sr.top+1,Math.max(0,sr.width-2),Math.max(0,sr.height-2));
      badge(ctx,Math.max(0,sr.left+2),Math.max(0,sr.top+2),String(item.id),color);
    }
    if(state.enabled&&state.hover&&state.hover.isConnected!==false){
      var hr=state.hover.getBoundingClientRect(),hc=colorFor(state.hover);
      ctx.fillStyle=colorFor(state.hover,.22);ctx.fillRect(hr.left,hr.top,hr.width,hr.height);
      ctx.strokeStyle=hc;ctx.lineWidth=3;ctx.strokeRect(hr.left+1.5,hr.top+1.5,Math.max(0,hr.width-3),Math.max(0,hr.height-3));
      var label=segment(state.hover)+'  '+Math.round(hr.width)+'×'+Math.round(hr.height);
      ctx.font='bold 11px monospace';ctx.textBaseline='middle';
      var tw=Math.min(w-4,ctx.measureText(label).width+12),ty=Math.max(2,hr.top-21),tx=Math.max(2,Math.min(hr.left,w-tw-2));
      ctx.fillStyle=hc;ctx.fillRect(tx,ty,tw,19);
      ctx.fillStyle='#05070e';ctx.fillText(label,tx+6,ty+10);
    }
  }
  function schedule(){if(state.frame)return;state.frame=requestAnimationFrame(function(){state.frame=0;draw()})}
  function point(x,y){
    // Touches arrive in device-independent pixels measured from the WebView. Once the page is pinch-zoomed
    // those no longer match CSS pixels, so convert through the visual viewport before hit testing.
    var vx=Number(x)||0,vy=Number(y)||0,v=window.visualViewport;
    if(!v||!v.scale||v.scale===1)return [vx,vy];
    return [(Number(v.offsetLeft)||0)+vx/v.scale,(Number(v.offsetTop)||0)+vy/v.scale];
  }
  function pick(el){
    if(!el||el.nodeType!==1||ui(el))return;
    var index=indexOf(el);
    state.hover=null;
    if(index>=0){
      var item=state.selection[index];
      emitDetails([detail(item.el,item.id)]);
    }else{
      state.counter++;
      state.selection.push({el:el,id:state.counter});
      emitSelection();
    }
    draw();
  }
  function reveal(el){try{el.scrollIntoView({block:'center',inline:'center'})}catch(e){}}
  var state={
    enabled:enabled,canvas:null,hover:null,frame:0,counter:0,selection:[],
    setEnabled:function(value){
      state.enabled=value;state.hover=null;
      document.documentElement.style.cursor=value?'crosshair':'';
      draw();
    },
    pickAt:function(x,y){var p=point(x,y);pick(document.elementFromPoint(p[0],p[1]))},
    hoverAt:function(x,y){if(!state.enabled)return;var p=point(x,y),el=document.elementFromPoint(p[0],p[1]);if(!el||ui(el))return;state.hover=el;schedule()},
    clearHover:function(){state.hover=null;schedule()},
    inspect:function(id){var item=find(id);if(!item)return;reveal(item.el);setTimeout(function(){emitDetails([detail(item.el,item.id)]);draw()},60)},
    inspectAll:function(){
      if(!state.selection.length)return;
      emitDetails(state.selection.map(function(item){return detail(item.el,item.id)}));
    },
    inspectSelector:function(value){
      try{
        var el=document.querySelector(value);
        if(!el)return;
        reveal(el);
        var index=indexOf(el);
        setTimeout(function(){emitDetails([detail(el,index>=0?state.selection[index].id:0)])},60);
      }catch(e){}
    },
    drop:function(id){state.selection=state.selection.filter(function(item){return item.id!==id});emitSelection();draw()},
    clear:function(){state.selection=[];state.counter=0;emitSelection();draw()},
    // Exposed so the performance run can name the element behind a layout shift without duplicating this logic.
    selectorFor:function(node){try{return (node&&node.nodeType===1)?selector(node):''}catch(e){return ''}}
  };
  window[key]=state;
  document.addEventListener('pointermove',function(e){if(state.enabled){state.hover=e.target;schedule()}},true);
  document.addEventListener('click',function(e){
    if(!state.enabled)return;
    var el=e.target;
    if(!el||el.nodeType!==1||ui(el))return;
    e.preventDefault();e.stopPropagation();e.stopImmediatePropagation();
    pick(el);
  },true);
  window.addEventListener('scroll',schedule,true);
  window.addEventListener('resize',schedule);
  if(window.MutationObserver&&document.body){
    new MutationObserver(function(){prune();schedule()}).observe(document.body,{childList:true,subtree:true});
  }
  draw();
}
"""
