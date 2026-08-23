#!/usr/bin/env node
// build.js — regenerate the design register, the answer form for every OPEN
// QUESTION across the authority docs (published as an Artifact for Ben to fill in).
//
//   node tools/session/register/build.js [out.html]
//
// questions.js is the canonical question set: one entry per document, each with
// its open questions, the evidence behind them, the options, and which option is
// recommended. Edit THAT, not the HTML — the HTML is generated.
//
// The published page is a LIVE DOC: radios and contenteditable fields are captured
// as edits, so answers reach the watching session. Deliberately no <textarea> and no
// <select> — neither is captured. Script-computed chrome sits in <artifact-local>.
//
// Republish to the SAME artifact url to keep answers in place.
const fs=require('fs');
const path=require('path');
const S=require(path.join(__dirname,'questions.js'));
const esc=s=>String(s).replace(/&(?!#?\w+;)/g,'&amp;');

let nav='', body='';
S.forEach(sec=>{
  nav += `<a class="rail-item" href="#${sec.doc}"><span class="rail-name">${sec.title}</span><span class="rail-count" data-count-for="${sec.doc}">0/${sec.qs.length}</span></a>\n`;
  body += `<section class="sec" id="${sec.doc}">
  <header class="sec-head">
    <p class="sec-file">${esc(sec.file)}</p>
    <h2 class="sec-title">${esc(sec.title)}</h2>
    <p class="sec-blurb">${esc(sec.blurb)}</p>
  </header>\n`;
  sec.qs.forEach(q=>{
    const key=`${sec.doc}-${q.n}`;
    const ids=q.ids.map(i=>`<span class="id">${i}</span>`).join('');
    const type=q.multi?'checkbox':'radio';
    const opts=q.opts.map((o,i)=>{
      const oid=`${key}-o${i}`;
      const rec=(!q.multi&&q.rec===i)||(q.multi&&q.rec===i)?'<span class="rec">suggested</span>':'';
      return `      <label class="opt" for="${oid}">
        <input type="${type}" id="${oid}" name="${key}" value="${i}" data-q="${key}" data-label="${esc(o).replace(/"/g,'&quot;')}">
        <span class="opt-body"><span class="opt-text">${esc(o)}</span>${rec}</span>
      </label>`;
    }).join('\n');
    body += `  <article class="q" data-key="${key}" data-multi="${q.multi?'1':'0'}">
    <div class="q-top">
      <span class="q-num">${sec.doc==='CROSS'?'':'Q'}${q.n}</span>
      <span class="q-ids">${ids}</span>
      <span class="q-state" data-state-for="${key}">open</span>
    </div>
    <h3 class="q-text">${q.q}</h3>
    <p class="q-ctx">${q.ctx}</p>
    ${q.blocks?`<p class="q-blocks"><span class="blocks-label">Blocks</span> ${esc(q.blocks)}</p>`:''}
    <div class="opts" role="group" aria-label="Options">
${opts}
    </div>
    <div class="note">
      <label class="note-label" for="${key}-note">${q.freeform?esc(q.freeform):'In your own words &mdash; optional, and it overrides the options above'}</label>
      <div class="note-field" id="${key}-note" contenteditable="true" role="textbox" aria-multiline="true" data-note="${key}"></div>
    </div>
  </article>\n`;
  });
  body += `</section>\n`;
});

const html=`<title>Io Design Register</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Spectral:ital,wght@0,400;0,600;1,400&family=Public+Sans:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>
:root{
  --ground:#F3F5F2; --surface:#FFFFFF; --surface-2:#EDF1EE;
  --ink:#191E1B; --ink-2:#3B4741; --muted:#5C6862; --faint:#8B968F;
  --rule:#D7DEDA; --rule-2:#C3CDC7;
  --accent:#2C6B59; --accent-soft:#E2EFE9; --accent-ink:#1E4C3F;
  --amber:#8F6414; --amber-soft:#F6EEDC;
  --shadow:0 1px 2px rgba(25,30,27,.05),0 8px 24px -12px rgba(25,30,27,.14);
  --maxw:46rem;
}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){
  --ground:#111513; --surface:#181D1A; --surface-2:#1E2521;
  --ink:#E7ECE8; --ink-2:#C6D0CA; --muted:#8B9992; --faint:#6C7A73;
  --rule:#262F2A; --rule-2:#33403A;
  --accent:#63BFA1; --accent-soft:#17302A; --accent-ink:#8FD6BD;
  --amber:#D2A244; --amber-soft:#2A2418;
  --shadow:0 1px 2px rgba(0,0,0,.3),0 8px 24px -12px rgba(0,0,0,.55);
}}
:root[data-theme="dark"]{
  --ground:#111513; --surface:#181D1A; --surface-2:#1E2521;
  --ink:#E7ECE8; --ink-2:#C6D0CA; --muted:#8B9992; --faint:#6C7A73;
  --rule:#262F2A; --rule-2:#33403A;
  --accent:#63BFA1; --accent-soft:#17302A; --accent-ink:#8FD6BD;
  --amber:#D2A244; --amber-soft:#2A2418;
  --shadow:0 1px 2px rgba(0,0,0,.3),0 8px 24px -12px rgba(0,0,0,.55);
}
*{box-sizing:border-box}
body{
  margin:0; background:var(--ground); color:var(--ink);
  font-family:"Public Sans",-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
  font-size:16px; line-height:1.6; -webkit-font-smoothing:antialiased;
}
code{font-family:"IBM Plex Mono",ui-monospace,SFMono-Regular,Menlo,monospace;font-size:.88em;background:var(--surface-2);padding:.1em .34em;border-radius:3px}
b,strong{font-weight:600;color:var(--ink)}
i,em{font-style:italic}

.wrap{display:grid;grid-template-columns:15rem minmax(0,1fr);gap:3rem;max-width:74rem;margin:0 auto;padding:0 1.75rem 8rem}
@media(max-width:64rem){.wrap{grid-template-columns:1fr;gap:0}}

/* masthead */
.mast{grid-column:1/-1;padding:4.5rem 0 2.5rem;border-bottom:1px solid var(--rule)}
.mast-eyebrow{font-family:"IBM Plex Mono",monospace;font-size:.72rem;letter-spacing:.14em;text-transform:uppercase;color:var(--accent);margin:0 0 1rem}
.mast h1{font-family:Spectral,Georgia,serif;font-weight:600;font-size:clamp(2.3rem,5.5vw,3.4rem);line-height:1.05;letter-spacing:-.015em;margin:0 0 1rem;text-wrap:balance}
.mast p{margin:0;max-width:38rem;color:var(--muted);font-size:1.02rem}
.mast .lead{color:var(--ink-2);font-size:1.1rem;margin-bottom:.85rem}

/* rail */
.rail{position:sticky;top:1.5rem;align-self:start;padding-top:2.5rem;display:flex;flex-direction:column;gap:.1rem}
@media(max-width:64rem){.rail{position:static;flex-direction:row;flex-wrap:wrap;gap:.4rem;padding:1.5rem 0;border-bottom:1px solid var(--rule)}}
.rail-item{display:flex;align-items:baseline;justify-content:space-between;gap:.6rem;padding:.45rem .6rem;border-radius:5px;text-decoration:none;color:var(--muted);font-size:.9rem;transition:background .12s,color .12s}
.rail-item:hover{background:var(--surface-2);color:var(--ink)}
.rail-name{font-weight:500}
.rail-count{font-family:"IBM Plex Mono",monospace;font-size:.74rem;color:var(--faint);font-variant-numeric:tabular-nums}
.rail-item[data-done="1"] .rail-count{color:var(--accent);font-weight:500}

/* sections */
.col{padding-top:2.5rem;min-width:0}
.sec{margin-bottom:4.5rem;scroll-margin-top:1.5rem}
.sec-head{margin-bottom:1.75rem}
.sec-file{font-family:"IBM Plex Mono",monospace;font-size:.74rem;color:var(--faint);margin:0 0 .45rem;word-break:break-all}
.sec-title{font-family:Spectral,Georgia,serif;font-weight:600;font-size:1.9rem;line-height:1.15;margin:0 0 .4rem;letter-spacing:-.01em}
.sec-blurb{margin:0;color:var(--muted);font-size:.95rem;max-width:var(--maxw)}

/* question card */
.q{background:var(--surface);border:1px solid var(--rule);border-radius:9px;padding:1.5rem 1.6rem;margin-bottom:1.1rem;max-width:var(--maxw);box-shadow:var(--shadow);transition:border-color .15s}
.q[data-answered="1"]{border-color:var(--accent);border-left-width:3px;padding-left:calc(1.6rem - 2px)}
.q-top{display:flex;align-items:center;gap:.55rem;flex-wrap:wrap;margin-bottom:.7rem}
.q-num{font-family:"IBM Plex Mono",monospace;font-size:.76rem;font-weight:500;color:var(--accent);letter-spacing:.04em}
.q-ids{display:flex;gap:.3rem;flex-wrap:wrap}
.id{font-family:"IBM Plex Mono",monospace;font-size:.7rem;color:var(--muted);background:var(--surface-2);border:1px solid var(--rule);padding:.08rem .34rem;border-radius:3px;white-space:nowrap}
.q-state{margin-left:auto;font-family:"IBM Plex Mono",monospace;font-size:.68rem;text-transform:uppercase;letter-spacing:.1em;color:var(--amber);background:var(--amber-soft);padding:.14rem .42rem;border-radius:3px}
.q[data-answered="1"] .q-state{color:var(--accent-ink);background:var(--accent-soft)}
.q-text{font-family:Spectral,Georgia,serif;font-weight:600;font-size:1.24rem;line-height:1.3;margin:0 0 .7rem;text-wrap:balance}
.q-ctx{margin:0 0 1rem;color:var(--ink-2);font-size:.95rem}
.q-blocks{margin:-.4rem 0 1rem;font-size:.87rem;color:var(--muted)}
.blocks-label{font-family:"IBM Plex Mono",monospace;font-size:.68rem;text-transform:uppercase;letter-spacing:.1em;color:var(--amber);margin-right:.4rem}

/* options */
.opts{display:flex;flex-direction:column;gap:.3rem;margin-bottom:1.05rem}
.opt{display:flex;gap:.7rem;align-items:flex-start;padding:.62rem .75rem;border:1px solid var(--rule);border-radius:6px;cursor:pointer;background:var(--ground);transition:border-color .12s,background .12s}
.opt:hover{border-color:var(--rule-2);background:var(--surface-2)}
.opt input{margin:.34rem 0 0;accent-color:var(--accent);flex:none;width:15px;height:15px;cursor:pointer}
.opt:has(input:checked){border-color:var(--accent);background:var(--accent-soft)}
.opt:has(input:focus-visible){outline:2px solid var(--accent);outline-offset:2px}
.opt-body{display:flex;gap:.5rem;align-items:baseline;flex-wrap:wrap;min-width:0}
.opt-text{font-size:.93rem;line-height:1.45}
.rec{font-family:"IBM Plex Mono",monospace;font-size:.64rem;text-transform:uppercase;letter-spacing:.09em;color:var(--muted);border:1px solid var(--rule-2);padding:.05rem .3rem;border-radius:3px;flex:none}

/* note */
.note{border-top:1px dashed var(--rule);padding-top:.9rem}
.note-label{display:block;font-size:.78rem;color:var(--faint);margin-bottom:.4rem}
.note-field{min-height:2.7rem;padding:.55rem .7rem;border:1px solid var(--rule);border-radius:6px;background:var(--ground);font-size:.93rem;line-height:1.55;color:var(--ink);outline:none;transition:border-color .12s}
.note-field:focus{border-color:var(--accent);background:var(--surface)}
.note-field:empty::before{content:"Type here…";color:var(--faint)}

/* action bar */
.bar{position:fixed;left:0;right:0;bottom:0;background:var(--surface);border-top:1px solid var(--rule);padding:.8rem 1.75rem;display:flex;align-items:center;gap:1.1rem;z-index:10;box-shadow:0 -6px 20px -14px rgba(0,0,0,.4)}
.bar-inner{max-width:74rem;margin:0 auto;width:100%;display:flex;align-items:center;gap:1.1rem;flex-wrap:wrap}
.prog{display:flex;align-items:center;gap:.7rem;min-width:0}
.prog-track{width:9rem;height:5px;background:var(--surface-2);border-radius:3px;overflow:hidden;flex:none}
.prog-fill{height:100%;width:0;background:var(--accent);border-radius:3px;transition:width .25s}
.prog-text{font-family:"IBM Plex Mono",monospace;font-size:.8rem;color:var(--muted);font-variant-numeric:tabular-nums;white-space:nowrap}
.btn{margin-left:auto;font-family:"Public Sans",sans-serif;font-size:.88rem;font-weight:500;padding:.5rem 1.05rem;border-radius:6px;border:1px solid var(--accent);background:var(--accent);color:var(--ground);cursor:pointer;transition:opacity .12s}
.btn:hover{opacity:.88}
.btn:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.btn.ghost{margin-left:0;background:transparent;color:var(--accent);border-color:var(--rule-2)}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
</style>

<div class="wrap">
  <header class="mast">
    <p class="mast-eyebrow">Project Io &middot; 41 open calls &middot; 22 August 2026</p>
    <h1>Every question the documents are still short of an answer on</h1>
    <p class="lead">Eight authority documents were written this week. Each one ends in questions nobody has made a call on &mdash; and those questions are the point of the documents.</p>
    <p>Pick an option, or write your own words in the field below it &mdash; your words override the option. Nothing here is binding; the recommendation marks are mine and are meant to be overruled. When you are done, <b>Copy all answers</b> puts everything on the clipboard in one block.</p>
  </header>

  <nav class="rail" aria-label="Documents">
${nav}  </nav>

  <main class="col">
${body}  </main>
</div>

<artifact-local>
<div class="bar">
  <div class="bar-inner">
    <div class="prog">
      <div class="prog-track"><div class="prog-fill" id="fill"></div></div>
      <span class="prog-text" id="ptext">0 of 41 answered</span>
    </div>
    <button class="btn ghost" id="clear" type="button">Clear all</button>
    <button class="btn" id="copy" type="button">Copy all answers</button>
  </div>
</div>
</artifact-local>

<script>
(function(){
  var SECS=${JSON.stringify(S.map(s=>({doc:s.doc,title:s.title,file:s.file,qs:s.qs.map(q=>({n:q.n,key:s.doc+'-'+q.n,q:q.q.replace(/<[^>]+>/g,''),ids:q.ids}))})))};
  var LS='io-design-register-v1';

  function answered(art){
    var checked=art.querySelectorAll('input:checked').length;
    var note=art.querySelector('.note-field');
    var txt=note?note.textContent.trim():'';
    return checked>0||txt.length>0;
  }
  function refresh(){
    var total=0,done=0;
    document.querySelectorAll('.sec').forEach(function(sec){
      var arts=sec.querySelectorAll('.q'),n=0;
      arts.forEach(function(a){
        var ok=answered(a);
        a.setAttribute('data-answered',ok?'1':'0');
        var st=a.querySelector('.q-state'); if(st) st.textContent=ok?'answered':'open';
        if(ok)n++;
      });
      total+=arts.length; done+=n;
      var c=document.querySelector('[data-count-for="'+sec.id+'"]');
      if(c){c.textContent=n+'/'+arts.length; c.parentElement.setAttribute('data-done',n===arts.length?'1':'0');}
    });
    var f=document.getElementById('fill'),t=document.getElementById('ptext');
    if(f)f.style.width=(total?done/total*100:0)+'%';
    if(t)t.textContent=done+' of '+total+' answered';
    save();
  }
  function save(){
    try{
      var st={r:{},n:{}};
      document.querySelectorAll('input:checked').forEach(function(i){
        var k=i.getAttribute('data-q'); (st.r[k]=st.r[k]||[]).push(i.value);
      });
      document.querySelectorAll('.note-field').forEach(function(d){
        var v=d.textContent.trim(); if(v) st.n[d.getAttribute('data-note')]=v;
      });
      localStorage.setItem(LS,JSON.stringify(st));
    }catch(e){}
  }
  function restore(){
    try{
      var raw=localStorage.getItem(LS); if(!raw)return;
      var st=JSON.parse(raw);
      Object.keys(st.r||{}).forEach(function(k){
        st.r[k].forEach(function(v){
          var el=document.querySelector('input[data-q="'+k+'"][value="'+v+'"]');
          if(el&&!el.checked)el.checked=true;
        });
      });
      Object.keys(st.n||{}).forEach(function(k){
        var d=document.querySelector('.note-field[data-note="'+k+'"]');
        if(d&&!d.textContent.trim())d.textContent=st.n[k];
      });
    }catch(e){}
  }
  function text(){
    var out=['Project Io - design register answers','Generated from the open questions of 22 August 2026.',''];
    SECS.forEach(function(sec){
      var lines=[];
      sec.qs.forEach(function(q){
        var art=document.querySelector('.q[data-key="'+q.key+'"]'); if(!art)return;
        var picks=[];
        art.querySelectorAll('input:checked').forEach(function(i){picks.push(i.getAttribute('data-label'));});
        var note=art.querySelector('.note-field');
        var txt=note?note.textContent.trim():'';
        if(!picks.length&&!txt)return;
        lines.push('Q'+q.n+'. '+q.q+'  ['+q.ids.join(', ')+']');
        if(picks.length)lines.push('    CHOSE: '+picks.join(' | '));
        if(txt)lines.push('    BEN: '+txt);
        lines.push('');
      });
      if(lines.length){out.push('## '+sec.title+'  ('+sec.file+')','');out=out.concat(lines);}
    });
    if(out.length<=3)out.push('(nothing answered yet)');
    return out.join('\\n');
  }

  document.addEventListener('change',function(e){ if(e.target.matches('input'))refresh(); });
  document.addEventListener('input',function(e){ if(e.target.classList&&e.target.classList.contains('note-field'))refresh(); });

  document.getElementById('copy').addEventListener('click',function(){
    var t=text(),b=this;
    function done(){b.textContent='Copied';setTimeout(function(){b.textContent='Copy all answers';},1600);}
    if(navigator.clipboard&&navigator.clipboard.writeText){navigator.clipboard.writeText(t).then(done,fallback);}else fallback();
    function fallback(){
      var ta=document.createElement('textarea');ta.value=t;ta.style.position='fixed';ta.style.opacity='0';
      document.body.appendChild(ta);ta.select();try{document.execCommand('copy');done();}catch(e){b.textContent='Copy failed';}
      document.body.removeChild(ta);
    }
  });
  document.getElementById('clear').addEventListener('click',function(){
    if(!confirm('Clear every answer on this page?'))return;
    document.querySelectorAll('input:checked').forEach(function(i){i.checked=false;});
    document.querySelectorAll('.note-field').forEach(function(d){d.textContent='';});
    try{localStorage.removeItem(LS);}catch(e){}
    refresh();
  });

  restore();
  refresh();
})();
</script>
`;
const out=process.argv[2]||path.join(__dirname,'register.html');
fs.writeFileSync(out,html);
console.log('register: '+S.length+' sections, '+S.reduce((n,x)=>n+x.qs.length,0)+' questions -> '+out+' ('+(html.length/1024).toFixed(1)+'KB)');
