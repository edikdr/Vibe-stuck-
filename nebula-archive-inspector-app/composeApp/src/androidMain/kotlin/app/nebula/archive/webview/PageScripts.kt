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
  try{
    var deltas=[],last=performance.now(),frames=0;
    await new Promise(function(resolve){
      function sample(now){
        if(frames>0)deltas.push(now-last);
        last=now;frames++;
        if(frames<=60)requestAnimationFrame(sample);else resolve();
      }
      requestAnimationFrame(sample);
    });
    var nav=performance.getEntriesByType('navigation')[0]||{},paints=performance.getEntriesByType('paint')||[],resources=performance.getEntriesByType('resource')||[];
    var fcp=null;for(var i=0;i<paints.length;i++){if(paints[i].name==='first-contentful-paint'){fcp=paints[i].startTime;break}}
    var total=0;for(var r=0;r<resources.length;r++)total+=resources[r].decodedBodySize||resources[r].transferSize||0;
    var average=deltas.length?deltas.reduce(function(a,b){return a+b},0)/deltas.length:0;
    var worst=deltas.length?Math.max.apply(Math,deltas):0;
    var slow=deltas.length?deltas.filter(function(v){return v>34}).length/deltas.length*100:0;
    NebulaInspector.performanceResult(JSON.stringify({
      runToken:runToken,
      responseStartMs:Number(nav.responseStart||0),
      domContentLoadedMs:Number(nav.domContentLoadedEventEnd||0),
      loadMs:Number(nav.loadEventEnd||nav.duration||0),
      firstContentfulPaintMs:fcp===null?-1:Number(fcp),
      averageFrameMs:average,
      worstFrameMs:worst,
      slowFramePercent:slow,
      domNodes:document.getElementsByTagName('*').length,
      resourceCount:resources.length,
      decodedResourceBytes:total
    }));
  }catch(e){
    NebulaInspector.performanceResult(JSON.stringify({runToken:runToken,loadMs:0,domNodes:document.getElementsByTagName('*').length}));
  }
}
"""
