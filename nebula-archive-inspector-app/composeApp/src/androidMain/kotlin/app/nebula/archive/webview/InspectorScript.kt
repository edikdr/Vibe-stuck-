package app.nebula.archive

/**
 * The inspector that runs inside the archived page.
 *
 * It never mutates the page: outlines, selection marks and labels are painted on one fixed canvas above the
 * document, so a site cannot be broken by being inspected, and nothing has to be restored afterwards.
 *
 * Picking is two-stage on purpose. The first tap marks an element and leaves the user in the page, the second
 * tap on the same element inspects it. That makes a multi-selection possible with the only input a phone has.
 *
 * Hit testing does not use the event target. A decorative layer usually carries `pointer-events:none`, so the
 * browser hands the touch to whatever sits behind it — normally the section's background — and the element
 * the user actually aimed at is never offered. Picking therefore builds the whole stack under the point
 * itself: the elements the browser hit, plus every `pointer-events:none` layer whose own box holds the
 * point — the only elements hit testing can never report — ordered by CSS painting order. The topmost of those wins, an ancestor is never substituted for it, and the rest of the
 * stack is reported so the user can choose another layer by hand. Layers that paint nothing where the point
 * is — a transparent pixel of an image, an empty full-viewport overlay — are offered last instead of first.
 */
internal const val INSPECTOR_SCRIPT = """
function(enabled){
  var key='__nebulaInspector';
  if(window[key]){window[key].setEnabled(enabled);return}
  var MAX_OUTLINES=1200;
  /** More layers than this under one point is a page nobody can choose from anyway. */
  var MAX_CANDIDATES=24;
  /** An element this close to the viewport in both axes is a backdrop, not a target. */
  var FULL_LAYER=.92;
  /** Below this alpha an image pixel shows whatever is behind it, so the image is not what was hit. */
  var CLEAR_ALPHA=.06;
  /** A style change can make a layer decorative without touching the DOM, so the decor set expires. */
  var DECOR_TTL=1500;
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
  /**
   * `pointer-events` is deliberately not consulted here: it describes what the mouse does to the page, not
   * whether a node can be inspected. Decor, overlays and pseudo layers have to stay pickable.
   */
  function selectable(el){
    if(!el||el.nodeType!==1||ui(el))return false;
    if(/^(script|style|meta|link|title|head|html|body|noscript|template)${'$'}/i.test(el.tagName))return false;
    var r=el.getBoundingClientRect();
    if(r.width<4||r.height<4)return false;
    var s=getComputedStyle(el);
    return s.display!=='none'&&s.visibility!=='hidden'&&Number(s.opacity||1)>0;
  }
  function visibleElements(){
    var out=[],nodes=allNodes(),w=innerWidth,h=innerHeight;
    for(var i=0;i<nodes.length&&out.length<MAX_OUTLINES;i++){
      var el=nodes[i];
      if(!selectable(el))continue;
      var r=el.getBoundingClientRect();
      if(r.bottom<0||r.top>h||r.right<0||r.left>w)continue;
      out.push([el,r]);
    }
    return out;
  }

  // ---- hit testing ------------------------------------------------------------------------------------

  /** The element list only changes when the DOM does, and picking walks it on every hover. */
  function allNodes(){
    if(!state.nodes||state.nodesDirty){state.nodes=document.body?slice(document.body.querySelectorAll('*')):[];state.nodesDirty=false}
    return state.nodes;
  }
  /**
   * The elements normal hit testing can never report: everything whose computed `pointer-events` is `none`.
   * A child that turns them back on is hit normally and comes from `elementsFromPoint`, so this set plus that
   * list is the complete stack — and it is short enough to sweep on every frame.
   */
  function decorNodes(){
    var now=Date.now();
    if(state.decor&&!state.decorDirty&&now-state.decorAt<DECOR_TTL)return state.decor;
    var nodes=allNodes(),out=[];
    for(var i=0;i<nodes.length;i++)if(getComputedStyle(nodes[i]).pointerEvents==='none')out.push(nodes[i]);
    state.decor=out;state.decorAt=now;state.decorDirty=false;
    return out;
  }
  function boxHit(el,x,y){
    // The bounding box is one cheap call and rejects nearly every node; only a hit pays for the exact rects,
    // which matter for wrapped inline elements whose bounding box covers space they do not paint.
    var b=el.getBoundingClientRect();
    if(x<b.left||x>b.right||y<b.top||y>b.bottom)return false;
    var rects=el.getClientRects&&el.getClientRects();
    if(rects&&rects.length>1){
      for(var i=0;i<rects.length;i++){
        var r=rects[i];
        if(x>=r.left&&x<=r.right&&y>=r.top&&y<=r.bottom)return true;
      }
      return false;
    }
    return true;
  }
  /** A layer scrolled or clipped out of its container is not under the point, whatever its own box says. */
  function clipped(el,x,y){
    var n=el.parentElement;
    while(n&&n!==document.body&&n!==document.documentElement){
      var s=getComputedStyle(n);
      if(s.overflow!=='visible'||s.overflowX!=='visible'||s.overflowY!=='visible'){
        var r=n.getBoundingClientRect();
        if(x<r.left||x>r.right||y<r.top||y>r.bottom)return true;
      }
      n=n.parentElement;
    }
    return false;
  }
  function contains(a,b){return a===b||!!(a.compareDocumentPosition(b)&16)}
  function zOf(s){var z=parseInt(s.zIndex,10);return isNaN(z)?0:z}
  function layered(s){return s.position!=='static'||s.transform!=='none'||s.filter!=='none'||(Number(s.opacity||1)<1)}
  /** Positive when [a] paints above [b]: an approximation of the CSS painting order, siblings included. */
  function above(a,b){
    if(a===b)return 0;
    if(contains(a,b))return -1;
    if(contains(b,a))return 1;
    var ca=chain(a),cb=chain(b),i=0;
    while(i<ca.length&&i<cb.length&&ca[i]===cb[i])i++;
    var na=ca[i],nb=cb[i];
    if(!na||!nb)return 0;
    var sa=getComputedStyle(na),sb=getComputedStyle(nb),za=zOf(sa),zb=zOf(sb);
    if(za!==zb)return za-zb;
    var la=layered(sa)?1:0,lb=layered(sb)?1:0;
    if(la!==lb)return la-lb;
    return (na.compareDocumentPosition(nb)&4)?-1:1;
  }
  function chain(el){var out=[],n=el;while(n&&n.nodeType===1){out.unshift(n);n=n.parentElement}return out}
  function ownText(el){
    var kids=el.childNodes;
    for(var i=0;i<kids.length;i++){var n=kids[i];if(n.nodeType===3&&String(n.nodeValue).trim())return true}
    return false;
  }
  function paintsItself(s){
    if(s.backgroundImage&&s.backgroundImage!=='none')return true;
    var bg=String(s.backgroundColor||'');
    if(bg&&bg!=='transparent'&&bg.replace(/\s/g,'')!=='rgba(0,0,0,0)')return true;
    if(s.borderStyle&&s.borderStyle!=='none'&&s.borderWidth&&s.borderWidth!=='0px')return true;
    return !!(s.boxShadow&&s.boxShadow!=='none');
  }
  /**
   * Alpha of the image pixel under the point, with `object-fit` taken into account. A cross-origin image
   * taints the sampler and throws; an image that cannot be read counts as opaque, so nothing is lost.
   */
  function imageAlpha(img,x,y){
    try{
      if(!img.complete||!img.naturalWidth||!img.naturalHeight)return 1;
      var r=img.getBoundingClientRect();
      if(r.width<1||r.height<1)return 1;
      var s=getComputedStyle(img),fit=s.objectFit||'fill',iw=img.naturalWidth,ih=img.naturalHeight,px,py;
      if(fit==='contain'||fit==='cover'||fit==='scale-down'){
        var scale=fit==='cover'?Math.max(r.width/iw,r.height/ih):Math.min(r.width/iw,r.height/ih);
        if(fit==='scale-down')scale=Math.min(scale,1);
        var ox=(r.width-iw*scale)/2,oy=(r.height-ih*scale)/2;
        px=(x-r.left-ox)/scale;py=(y-r.top-oy)/scale;
        if(px<0||py<0||px>=iw||py>=ih)return 0;
      }else if(fit==='none'){
        px=x-r.left-(r.width-iw)/2;py=y-r.top-(r.height-ih)/2;
        if(px<0||py<0||px>=iw||py>=ih)return 0;
      }else{
        px=(x-r.left)/r.width*iw;py=(y-r.top)/r.height*ih;
      }
      var c=state.sampler||(state.sampler=document.createElement('canvas'));
      c.width=1;c.height=1;
      var ctx=c.getContext('2d',{willReadFrequently:true});
      if(!ctx)return 1;
      ctx.clearRect(0,0,1,1);
      ctx.drawImage(img,Math.min(iw-1,Math.max(0,Math.floor(px))),Math.min(ih-1,Math.max(0,Math.floor(py))),1,1,0,0,1,1);
      return ctx.getImageData(0,0,1,1).data[3]/255;
    }catch(e){return 1}
  }
  /**
   * 0 for a real target, 1 for a background layer of a section, 2 for a layer that paints nothing at all
   * under the point. Only the order of the offer changes — no candidate is ever dropped, so a click on empty
   * space still lands on the background that owns it.
   */
  function tierOf(el,x,y){
    if(el.tagName==='IMG'&&imageAlpha(el,x,y)<CLEAR_ALPHA)return 2;
    if(ownText(el))return 0;
    var r=el.getBoundingClientRect(),s=getComputedStyle(el);
    var full=r.width>=innerWidth*FULL_LAYER&&r.height>=innerHeight*FULL_LAYER;
    var inset=(s.position==='absolute'||s.position==='fixed')&&s.top==='0px'&&s.right==='0px'&&s.bottom==='0px'&&s.left==='0px';
    if(!full&&!inset)return 0;
    return paintsItself(s)?1:2;
  }
  /**
   * Every inspectable element under the point, topmost first. `elementsFromPoint` alone is not enough: it
   * obeys `pointer-events`, so it reports the background instead of the decoration lying on top of it. The
   * geometric sweep adds those layers back, and the painting order puts them where they visually are.
   */
  function stackAt(x,y){
    var found=[],seen=[];
    function add(el,trusted){
      if(!el||el.nodeType!==1||seen.indexOf(el)>=0)return;
      seen.push(el);
      if(!trusted&&(!boxHit(el,x,y)||clipped(el,x,y)))return;
      if(!selectable(el))return;
      found.push(el);
    }
    var hit=document.elementsFromPoint?slice(document.elementsFromPoint(x,y)):[document.elementFromPoint(x,y)];
    for(var i=0;i<hit.length;i++)add(hit[i],true);
    var nodes=decorNodes();
    for(var j=0;j<nodes.length&&found.length<MAX_CANDIDATES;j++)add(nodes[j],false);
    var ranked=found.map(function(el){return {el:el,tier:tierOf(el,x,y)}});
    rank(ranked);
    ranked.sort(function(a,b){return a.tier!==b.tier?a.tier-b.tier:above(b.el,a.el)});
    return ranked;
  }
  /**
   * A container inherits the weakest rank of the layers it holds, so demoting a background can never lift it
   * above its own child. Layers that paint nothing are already last and are not inherited from.
   */
  function rank(list){
    for(var i=0;i<list.length;i++){
      var tier=list[i].tier;
      if(tier===2)continue;
      for(var j=0;j<list.length;j++){
        var other=list[j];
        if(j===i||other.tier===2||other.tier<=tier)continue;
        if(contains(list[i].el,other.el))tier=other.tier;
      }
      list[i].tier=tier;
    }
  }
  function candidateOf(entry,index){
    var el=entry.el,r=el.getBoundingClientRect(),t=textOf(el,44);
    return {
      index:index,selector:selector(el),tag:el.tagName.toLowerCase(),label:segment(el)+(t?' · '+t:''),
      depth:depth(el),width:r.width,height:r.height,tier:entry.tier,
      decor:getComputedStyle(el).pointerEvents==='none',selected:indexOf(el)>=0
    };
  }
  function emitCandidates(){
    if(!window.NebulaInspector||!NebulaInspector.candidates)return;
    NebulaInspector.candidates(JSON.stringify(state.candidates.map(candidateOf)));
  }
  function setCandidates(stack){state.candidates=stack;emitCandidates()}

  // ---- inspection -------------------------------------------------------------------------------------

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
    if(s.pointerEvents==='none')always('pointer-events',s.pointerEvents);
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

  // ---- painting ---------------------------------------------------------------------------------------

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
    // The hover frame is drawn around the element a tap would pick, so a miss is visible before the tap.
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

  // ---- picking ----------------------------------------------------------------------------------------

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
  /** Picks the topmost layer under the point. An ancestor is never chosen in place of it. */
  function pickPoint(x,y){
    state.hoverPoint=null;
    var stack=stackAt(x,y);
    setCandidates(stack);
    if(stack.length)pick(stack[0].el);else draw();
  }
  /** Hovering runs the same hit test a tap would, at most once per frame, so the outline cannot disagree. */
  function hoverPoint(x,y){
    state.hoverPoint=[x,y];
    if(state.hoverFrame)return;
    state.hoverFrame=requestAnimationFrame(function(){
      state.hoverFrame=0;
      var p=state.hoverPoint;
      if(!p)return;
      var stack=stackAt(p[0],p[1]);
      state.hover=stack.length?stack[0].el:null;
      draw();
    });
  }
  function reveal(el){try{el.scrollIntoView({block:'center',inline:'center'})}catch(e){}}
  var state={
    enabled:enabled,canvas:null,hover:null,frame:0,counter:0,selection:[],candidates:[],
    nodes:null,nodesDirty:true,decor:null,decorDirty:true,decorAt:0,sampler:null,hoverPoint:null,hoverFrame:0,
    setEnabled:function(value){
      state.enabled=value;state.hover=null;state.hoverPoint=null;
      if(!value)setCandidates([]);
      document.documentElement.style.cursor=value?'crosshair':'';
      draw();
    },
    pickAt:function(x,y){var p=point(x,y);pickPoint(p[0],p[1])},
    hoverAt:function(x,y){if(!state.enabled)return;var p=point(x,y);hoverPoint(p[0],p[1])},
    clearHover:function(){state.hover=null;state.hoverPoint=null;schedule()},
    /** Picks another layer of the stack the last point reported, chosen by the user instead of guessed. */
    pickCandidate:function(index){
      var entry=state.candidates[index];
      if(!entry||entry.el.isConnected===false)return;
      pick(entry.el);
      emitCandidates();
    },
    /** Outlines a candidate while the user moves through the list, so the choice is visible before it is made. */
    highlightCandidate:function(index){
      var entry=state.candidates[index];
      state.hover=entry&&entry.el.isConnected!==false?entry.el:null;
      schedule();
    },
    dismissCandidates:function(){state.hover=null;setCandidates([]);schedule()},
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
    clear:function(){state.selection=[];state.counter=0;setCandidates([]);emitSelection();draw()},
    // Exposed so the performance run can name the element behind a layout shift without duplicating this logic.
    selectorFor:function(node){try{return (node&&node.nodeType===1)?selector(node):''}catch(e){return ''}}
  };
  window[key]=state;
  // Pointer coordinates, not the pointer target: the target of a touch over a `pointer-events:none` layer is
  // the element behind it, which is exactly the miss this inspector exists to avoid.
  document.addEventListener('pointermove',function(e){if(state.enabled&&!ui(e.target))hoverPoint(e.clientX,e.clientY)},true);
  document.addEventListener('click',function(e){
    if(!state.enabled)return;
    if(e.target&&e.target.nodeType===1&&ui(e.target))return;
    e.preventDefault();e.stopPropagation();e.stopImmediatePropagation();
    pickPoint(e.clientX,e.clientY);
  },true);
  window.addEventListener('scroll',schedule,true);
  window.addEventListener('resize',schedule);
  if(window.MutationObserver&&document.body){
    new MutationObserver(function(){state.nodesDirty=true;state.decorDirty=true;prune();schedule()}).observe(document.body,{childList:true,subtree:true});
  }
  draw();
}
"""
