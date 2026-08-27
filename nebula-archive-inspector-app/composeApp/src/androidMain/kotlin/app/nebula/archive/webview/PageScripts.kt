package app.nebula.archive

/**
 * Scripts injected into every archived page that is previewed.
 *
 * [PAGE_COMPATIBILITY_SCRIPT] repairs the gap between what a site expects from a browser and what an Android
 * WebView delivers: missing clicks on ordinary controls, modals and drawers that open but never paint, and
 * scroll-reveal animations whose observer never fires. Every forced style is recorded and restored, so a
 * repaired layer cannot keep an override once it closes.
 *
 * [PERFORMANCE_SCRIPT] measures the page in place: navigation timing, first contentful paint, 60 frames of
 * pacing, DOM size and resource weight.
 */
internal const val PAGE_COMPATIBILITY_SCRIPT = """
function(){
  if(window.__nebulaInteractionCompatibility)return;
  var state={down:null,touch:null,lastClickAt:0,timer:0,repairTimer:0,repairLater:0,repaired:[]};
  var selector='button,a[href],input:not([type="hidden"]),select,textarea,summary,label,[role="button"],[role="link"],[role="menuitem"],[role="tab"],[tabindex]:not([tabindex="-1"]),[aria-controls],[aria-haspopup],[data-action],[onclick]';
  var openQuery='dialog[open],[aria-modal="true"]:not([aria-hidden="true"]),[role="dialog"][aria-hidden="false"],.modal.open,.modal.active,.modal.show,.modal.is-open,.popup.open,.popup.active,.popup.show,.popup.is-open,.dialog.open,.dialog.active,.overlay.open,.overlay.active,.modal-overlay.open,[data-state="open"],[data-open="true"]';
  var contentQuery='.modal-content,.modal__content,.dialog-content,.dialog__content,.popup-content,.popup__content,[class*="modal-content"],[class*="modal__content"],[class*="dialog-content"],[class*="popup-content"]';
  var panelQuery='.music-player.open,.drawer.open,.drawer.active,.panel.open,[class*="drawer"].open,[class*="player"].open';
  var drawerQuery='.player-drawer,.drawer-content,.drawer__content,[class*="drawer-content"],[class*="drawer__content"]';
  function interactive(node){return node&&node.nodeType===1?(node.closest?node.closest(selector):node):null}
  function tapTarget(node){var explicit=interactive(node);if(explicit)return explicit;return node&&node.nodeType===1&&node!==document.body&&node!==document.documentElement?node:null}
  function clear(){state.down=null;if(state.timer){clearTimeout(state.timer);state.timer=0}}
  function visible(node){if(!node||!node.getBoundingClientRect)return false;var r=node.getBoundingClientRect(),s=getComputedStyle(node);return r.width>20&&r.height>20&&s.display!=='none'}
  function faded(style){return Number(style.opacity||1)<=.02||style.visibility==='hidden'}
  // Each repaired element carries its own saved declarations, so a repair cycle never rescans the register.
  function force(el,name,value){
    if(!el.__nebulaRepair){el.__nebulaRepair={};state.repaired.push(el)}
    var saved=el.__nebulaRepair;
    if(!(name in saved))saved[name]=[el.style.getPropertyValue(name),el.style.getPropertyPriority(name)];
    el.style.setProperty(name,value,'important');
  }
  function restore(el){
    var saved=el.__nebulaRepair;if(!saved)return;
    for(var name in saved){
      var before=saved[name];
      if(before[0])el.style.setProperty(name,before[0],before[1]);else el.style.removeProperty(name);
    }
  }
  function forget(el){try{delete el.__nebulaRepair}catch(ignore){el.__nebulaRepair=null}}
  function releaseStale(roots){
    for(var i=state.repaired.length-1;i>=0;i--){
      var el=state.repaired[i],attached=el.isConnected!==false,keep=false;
      if(attached)for(var j=0;j<roots.length;j++){if(roots[j]===el||roots[j].contains(el)){keep=true;break}}
      if(keep)continue;
      if(attached)restore(el);
      forget(el);
      state.repaired.splice(i,1);
    }
  }
  function openLayers(){
    var found=[];
    function push(node){if(node&&found.indexOf(node)<0)found.push(node)}
    try{Array.prototype.slice.call(document.querySelectorAll(openQuery)).forEach(push)}catch(ignore){}
    try{Array.prototype.slice.call(document.querySelectorAll(panelQuery)).forEach(push)}catch(ignore){}
    try{Array.prototype.slice.call(document.querySelectorAll('[aria-expanded="true"]')).forEach(function(trigger){
      var id=trigger.getAttribute('aria-controls'),panel=id?document.getElementById(id):null;
      if(!panel&&trigger.closest)panel=trigger.closest('.open,.active,.show,.is-open,[data-state="open"]');
      push(panel);
    })}catch(ignore){}
    return found.filter(function(node){
      // `data-state="open"` is also carried by the trigger of a menu or accordion. Restacking a control is
      // never useful and can lift a button above the very layer it opened.
      if(node.matches&&node.matches('button,a[href],summary,input,select,textarea'))return false;
      return visible(node);
    }).slice(0,40);
  }
  function repairOpenLayers(){
    var layers=openLayers();
    releaseStale(layers);
    layers.forEach(function(layer){
      var ls=getComputedStyle(layer);
      force(layer,'isolation','isolate');
      force(layer,'content-visibility','visible');
      force(layer,'contain','none');
      force(layer,'z-index','2147483000');
      if(faded(ls)){force(layer,'opacity','1');force(layer,'visibility','visible')}
      var candidates=[];try{candidates=Array.prototype.slice.call(layer.querySelectorAll(contentQuery)).slice(0,20)}catch(ignore){}
      candidates.forEach(function(content){
        var r=content.getBoundingClientRect();if(r.width<40||r.height<24)return;
        var cs=getComputedStyle(content);
        force(content,'z-index','2147483001');
        force(content,'content-visibility','visible');
        force(content,'backface-visibility','visible');
        // Opacity and visibility can be forced without moving anything. `transform` cannot: dialog content is
        // centred with translate(-50%,-50%) far more often than it is animated in, and clearing that rule
        // throws the whole modal off screen, which is exactly what "the modal does not show" looks like.
        if(faded(cs)){force(content,'opacity','1');force(content,'visibility','visible')}
      });
      repairPanel(layer);
    });
  }
  function repairPanel(panel){
    var drawer=null;try{drawer=panel.querySelector(drawerQuery)}catch(ignore){}
    if(!drawer)return;
    var ps=getComputedStyle(panel);
    force(panel,'content-visibility','visible');
    force(panel,'contain','none');
    force(panel,'backface-visibility','visible');
    force(panel,'-webkit-backdrop-filter','none');
    force(panel,'backdrop-filter','none');
    force(drawer,'display','block');
    force(drawer,'visibility','visible');
    force(drawer,'opacity','1');
    force(drawer,'content-visibility','visible');
    var trigger=panel.querySelector('[aria-expanded="true"]');
    var rect=panel.getBoundingClientRect(),triggerHeight=trigger?trigger.getBoundingClientRect().height:0;
    if(rect.height<=triggerHeight+32){
      force(panel,'height','calc(100vh - 16px)');
      force(panel,'max-height','calc(100vh - 16px)');
      force(panel,'top','8px');
      force(panel,'bottom','8px');
    }
    // A fixed drawer that Chromium never re-rastered keeps stale layer bounds. Promote it to its own layer,
    // but only when it has no transform of its own, so an in-flight animation is never overwritten.
    if(ps.position==='fixed'&&ps.transform==='none')force(panel,'transform','translateZ(0)');
    void panel.offsetHeight;
  }
  function scheduleRepair(){if(state.repairTimer)clearTimeout(state.repairTimer);if(state.repairLater)clearTimeout(state.repairLater);state.repairTimer=setTimeout(repairOpenLayers,420);state.repairLater=setTimeout(repairOpenLayers,900)}
  function revealViewportContent(){
    var nodes=[];try{nodes=Array.prototype.slice.call(document.querySelectorAll('.reveal:not(.in),[data-aos]:not(.aos-animate),.scroll-reveal:not(.visible),.animate-on-scroll:not(.visible),.fade-in:not(.visible)'))}catch(ignore){}
    // `active` is deliberately not added here: sites reuse it for the current tab, slide or nav item, so
    // marking every revealed section active lights up several tabs at once and breaks the page styling.
    nodes.slice(0,500).forEach(function(node){var r=node.getBoundingClientRect();if(r.bottom<0||r.top>innerHeight||r.right<0||r.left>innerWidth)return;node.classList.add('in','visible','is-visible','aos-animate');setTimeout(function(){if(!node.isConnected)return;var s=getComputedStyle(node),b=node.getBoundingClientRect();if(b.bottom<0||b.top>innerHeight)return;if(Number(s.opacity||1)<=.02||s.visibility==='hidden'){node.style.setProperty('opacity','1','important');node.style.setProperty('visibility','visible','important');node.style.setProperty('transform','none','important')}},700)})
  }
  var revealFrame=0;function scheduleReveal(){if(revealFrame)return;revealFrame=requestAnimationFrame(function(){revealFrame=0;revealViewportContent()})}
  document.addEventListener('click',function(event){
    state.lastClickAt=performance.now();scheduleRepair();
  },true);
  window.addEventListener('scroll',scheduleReveal,{passive:true});window.addEventListener('resize',scheduleReveal);setTimeout(revealViewportContent,250);
  document.addEventListener('pointerdown',function(event){
    if(event.button!==undefined&&event.button!==0)return;
    var target=tapTarget(event.target);if(!target||target.disabled)return;
    clear();state.down={target:target,x:event.clientX,y:event.clientY,at:performance.now(),pointerId:event.pointerId};
  },true);
  document.addEventListener('pointermove',function(event){
    var down=state.down;if(!down||down.pointerId!==event.pointerId)return;
    if(Math.abs(event.clientX-down.x)>12||Math.abs(event.clientY-down.y)>12)clear();
  },true);
  document.addEventListener('pointercancel',clear,true);
  document.addEventListener('pointerup',function(event){
    var down=state.down;state.down=null;
    if(!down||down.pointerId!==event.pointerId||performance.now()-down.at>1200)return;
    if(Math.abs(event.clientX-down.x)>12||Math.abs(event.clientY-down.y)>12)return;
    state.timer=setTimeout(function(){
      state.timer=0;
      // Any click observed after this gesture started means the WebView delivered it itself. Comparing
      // timestamps instead of guessing whether the click landed on the same node is what keeps a modal from
      // being opened and immediately toggled shut again by a duplicated synthetic click.
      if(state.lastClickAt>down.at)return;
      try{down.target.click()}catch(ignore){}
    },180);
  },true);
  if(!window.PointerEvent){
  document.addEventListener('touchstart',function(event){
    if(!event.touches||event.touches.length!==1)return;var point=event.touches[0],target=tapTarget(event.target);if(!target||target.disabled)return;
    state.touch={target:target,x:point.clientX,y:point.clientY,at:performance.now()};
  },{capture:true,passive:true});
  document.addEventListener('touchmove',function(event){
    if(!state.touch||!event.touches||event.touches.length!==1)return;var point=event.touches[0];if(Math.abs(point.clientX-state.touch.x)>12||Math.abs(point.clientY-state.touch.y)>12)state.touch=null;
  },{capture:true,passive:true});
  document.addEventListener('touchcancel',function(){state.touch=null},true);
  document.addEventListener('touchend',function(){
    var touch=state.touch;state.touch=null;if(!touch||performance.now()-touch.at>1200||state.down)return;
    setTimeout(function(){if(state.lastClickAt>touch.at)return;try{touch.target.click()}catch(ignore){}},220);
  },{capture:true,passive:true});
  }
  if(window.HTMLDialogElement&&HTMLDialogElement.prototype&&!HTMLDialogElement.prototype.showModal){
    HTMLDialogElement.prototype.showModal=function(){this.setAttribute('open','');this.style.display='block'};
  }
  if(window.HTMLElement&&HTMLElement.prototype&&!HTMLElement.prototype.showPopover){
    HTMLElement.prototype.showPopover=function(){this.hidden=false;this.setAttribute('data-nebula-popover-open','');this.style.display='block'};
    HTMLElement.prototype.hidePopover=function(){this.removeAttribute('data-nebula-popover-open');this.style.display='none'};
    HTMLElement.prototype.togglePopover=function(force){var open=this.hasAttribute('data-nebula-popover-open');if(force===true||(!open&&force!==false))this.showPopover();else this.hidePopover();return !open};
  }
  var style=document.createElement('style');
  style.setAttribute('data-nebula-ui','interaction');
  style.textContent=selector+'{touch-action:manipulation}';
  (document.head||document.documentElement).appendChild(style);
  if(window.MutationObserver&&document.documentElement)new MutationObserver(scheduleRepair).observe(document.documentElement,{subtree:true,childList:true,attributes:true,attributeFilter:['class','open','aria-hidden','data-state','data-open']});
  window.__nebulaInteractionCompatibility=state;
}
"""

internal const val PERFORMANCE_SCRIPT = """
async function(runToken){
  function selectorOf(node){
    try{ var i=window.__nebulaInspector; return (node&&i&&i.selectorFor)?i.selectorFor(node):'' }catch(e){ return '' }
  }
  function label(node){
    if(!node||!node.tagName)return '';
    var s=node.tagName.toLowerCase();
    if(node.id)return s+'#'+node.id;
    var c=(node.className&&node.className.baseVal!==undefined)?node.className.baseVal:node.className;
    if(typeof c==='string'&&c.trim())s+='.'+c.trim().split(/\s+/).slice(0,2).join('.');
    return s;
  }
  function resourcePath(value){
    try{
      var url=new URL(value,location.href);
      if(url.host!==location.host)return url.href;
      return decodeURIComponent(url.pathname.replace(/^\//,''));
    }catch(e){ return String(value||'') }
  }
  function observe(type,sink){
    try{
      var observer=new PerformanceObserver(function(list){
        list.getEntries().forEach(function(entry){ sink.push(entry) });
      });
      observer.observe({type:type,buffered:true});
      return observer;
    }catch(e){ return null }
  }
  try{
    var shifts=[],tasks=[];
    var shiftObserver=observe('layout-shift',shifts);
    var taskObserver=observe('longtask',tasks);

    // The frames right after load are layout and paint finishing; counting them as jank blames every page.
    var deltas=[],last=performance.now(),frames=0,warmup=5;
    await new Promise(function(resolve){
      function sample(now){
        if(frames>warmup)deltas.push(now-last);
        last=now;frames++;
        if(frames<=60+warmup)requestAnimationFrame(sample);else resolve();
      }
      requestAnimationFrame(sample);
    });
    if(shiftObserver)shiftObserver.disconnect();
    if(taskObserver)taskObserver.disconnect();

    var nav=performance.getEntriesByType('navigation')[0]||{};
    var paints=performance.getEntriesByType('paint')||[];
    var resources=performance.getEntriesByType('resource')||[];
    var fcp=null;
    for(var i=0;i<paints.length;i++){ if(paints[i].name==='first-contentful-paint'){ fcp=paints[i].startTime;break } }

    var bytesByUrl={},total=0,scriptBytes=0;
    for(var r=0;r<resources.length;r++){
      var entry=resources[r];
      var size=entry.decodedBodySize||entry.transferSize||0;
      total+=size;
      bytesByUrl[entry.name]=size;
      if(entry.initiatorType==='script'||/\.(m|c)?js(\?|${'$'})/i.test(entry.name))scriptBytes+=size;
    }
    function bytesOf(value){
      if(!value)return 0;
      if(bytesByUrl[value])return bytesByUrl[value];
      try{ var absolute=new URL(value,location.href).href; return bytesByUrl[absolute]||0 }catch(e){ return 0 }
    }

    var average=deltas.length?deltas.reduce(function(a,b){return a+b},0)/deltas.length:0;
    var worstFrame=deltas.length?Math.max.apply(Math,deltas):0;
    var slow=deltas.length?deltas.filter(function(v){return v>34}).length/deltas.length*100:0;

    var findings=[];
    function add(kind,fields){
      if(findings.length>=20)return;
      fields=fields||{};
      findings.push({
        kind:kind,
        target:fields.target||'',
        selector:fields.selector||'',
        note:fields.note||'',
        value:Number(fields.value||0),
        extra:Number(fields.extra||0)
      });
    }

    // Layout shifts, attributed to the element that moved.
    var cls=0,worstShift=null;
    for(var s=0;s<shifts.length;s++){
      var shift=shifts[s];
      if(shift.hadRecentInput)continue;
      cls+=shift.value||0;
      if(!worstShift||(shift.value||0)>(worstShift.value||0))worstShift=shift;
    }
    if(cls>0.05){
      var moved=null,distance=0;
      if(worstShift&&worstShift.sources&&worstShift.sources.length){
        var source=worstShift.sources[0];
        moved=source.node||null;
        if(source.previousRect&&source.currentRect){
          distance=Math.max(
            Math.abs(source.currentRect.top-source.previousRect.top),
            Math.abs(source.currentRect.left-source.previousRect.left)
          );
        }
      }
      add('LayoutShift',{target:label(moved),selector:selectorOf(moved),value:cls,extra:distance});
    }

    // Long tasks block every interaction while they run.
    if(tasks.length){
      var taskTotal=0,taskWorst=0;
      for(var t=0;t<tasks.length;t++){
        taskTotal+=tasks[t].duration||0;
        if((tasks[t].duration||0)>taskWorst)taskWorst=tasks[t].duration||0;
      }
      if(taskTotal>150)add('LongTask',{value:taskWorst,extra:tasks.length});
    }

    // Synchronous scripts and stylesheets in the head delay the first paint.
    try{
      var head=document.head||document;
      Array.prototype.slice.call(head.querySelectorAll('script[src]')).slice(0,8).forEach(function(node){
        if(node.async||node.defer||node.type==='module')return;
        add('RenderBlocking',{target:resourcePath(node.src),selector:selectorOf(node),value:bytesOf(node.src)});
      });
    }catch(e){}

    // Images decoded far larger than they are painted, and images that reserve no space.
    try{
      var oversized=[];
      Array.prototype.slice.call(document.images).slice(0,200).forEach(function(image){
        var width=image.clientWidth,height=image.clientHeight;
        var source=image.currentSrc||image.src;
        if(!source)return;
        if(width&&height&&image.naturalWidth&&image.naturalHeight){
          var ratio=window.devicePixelRatio||1;
          var neededWidth=width*ratio,neededHeight=height*ratio;
          if(image.naturalWidth>neededWidth*1.5&&image.naturalHeight>neededHeight*1.5){
            var wasted=1-(neededWidth*neededHeight)/(image.naturalWidth*image.naturalHeight);
            oversized.push({
              target:resourcePath(source),
              selector:selectorOf(image),
              value:bytesOf(source),
              extra:wasted,
              note:image.naturalWidth+'×'+image.naturalHeight+' → '+Math.round(width)+'×'+Math.round(height)
            });
          }
        }
        if(!image.getAttribute('width')&&!image.getAttribute('height')){
          var style=getComputedStyle(image);
          if(!style.aspectRatio||style.aspectRatio==='auto'){
            add('MissingDimensions',{
              target:resourcePath(source),
              selector:selectorOf(image),
              note:image.naturalWidth?image.naturalWidth+'×'+image.naturalHeight:''
            });
          }
        }
      });
      oversized.sort(function(a,b){return b.value-a.value});
      oversized.slice(0,5).forEach(function(item){ add('OversizedImage',item) });
    }catch(e){}

    // The heaviest single downloads of the page.
    try{
      var heavy=resources.slice().sort(function(a,b){
        return (b.decodedBodySize||b.transferSize||0)-(a.decodedBodySize||a.transferSize||0);
      });
      for(var h=0;h<heavy.length&&h<3;h++){
        var size=heavy[h].decodedBodySize||heavy[h].transferSize||0;
        if(size<300000)break;
        add('HeavyResource',{target:resourcePath(heavy[h].name),value:size});
      }
    }catch(e){}

    // Properties that force their own compositing layer are the closest honest signal to GPU cost.
    try{
      var costly=0,nodes=document.body?document.body.querySelectorAll('*'):[];
      for(var n=0;n<nodes.length&&n<2000;n++){
        var computed=getComputedStyle(nodes[n]);
        if((computed.willChange&&computed.willChange!=='auto')||
           (computed.filter&&computed.filter!=='none')||
           (computed.backdropFilter&&computed.backdropFilter!=='none')){
          costly++;
        }
      }
      if(costly>12)add('CompositingCost',{value:costly});
    }catch(e){}

    NebulaInspector.performanceResult(JSON.stringify({
      runToken:runToken,
      responseStartMs:Number(nav.responseStart||0),
      domContentLoadedMs:Number(nav.domContentLoadedEventEnd||0),
      loadMs:Number(nav.loadEventEnd||nav.duration||0),
      firstContentfulPaintMs:fcp===null?-1:Number(fcp),
      averageFrameMs:average,
      worstFrameMs:worstFrame,
      slowFramePercent:slow,
      domNodes:document.getElementsByTagName('*').length,
      resourceCount:resources.length,
      decodedResourceBytes:total,
      scriptBytes:scriptBytes,
      layoutShiftScore:cls,
      longTaskCount:tasks.length,
      longTaskTotalMs:tasks.reduce(function(a,b){return a+(b.duration||0)},0),
      findings:findings
    }));
  }catch(e){
    NebulaInspector.performanceResult(JSON.stringify({
      runToken:runToken,loadMs:0,domNodes:document.getElementsByTagName('*').length,findings:[]
    }));
  }
}
"""
