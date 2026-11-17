import fs from 'node:fs';
import assert from 'node:assert/strict';
import {instantiate} from './test_host.mjs';
const mod=new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const result={build:process.argv[2],checks:[],pass:false};
function check(name,pass,detail){result.checks.push({name,pass:!!pass,detail});console.log(pass?'PASS':'FAIL',name,JSON.stringify(detail));}
function session(){
 const files=new Map();let x=instantiate(mod,files).exports;x.vibe_wasm_init(192000);x.vibe_wasm_event(18,0,0,0,0);
 const patch=files.get('patch.bin').slice(),v=new DataView(patch.buffer,patch.byteOffset,patch.length);
 v.setInt32(12,500,true);v.setInt32(28,220,true);v.setInt32(2088,0,true);patch[2288]=0;patch[2100]=0;
 for(let i=0;i<6;i++){const t=56+88*i;v.setInt32(t+24,1,true);v.setInt32(t+28,1,true);v.setInt32(t+32,0,true);v.setInt32(t+36,1000,true);v.setInt32(t+40,48+i,true);v.setInt32(t+48,0,true);v.setInt32(t+52,1,true);v.setInt32(t+60,1,true);patch[1292+132*i]=0;v.setInt32(652+104*i+8,0,true);}
 files.set('patch.bin',patch);const e=instantiate(mod,files).exports;e.vibe_wasm_init(192000);
 let t=0;const msgs=[];
 const cstr=p=>{const a=new Uint8Array(e.memory.buffer);let s='';for(;a[p];p++)s+=String.fromCharCode(a[p]);return s;};
 const ids=Array.from({length:e.vibe_wasm_param_count()},(_,i)=>cstr(new Uint32Array(e.memory.buffer,e.vibe_wasm_param_info(i),1)[0]));
 const ev=(type,id=0,down=0,value=0,midi=0)=>e.vibe_wasm_event(type,id,down,value,midi);
 const set=(id,value)=>e.vibe_wasm_param_set(ids.indexOf(id),value);
 function drain(){const b=new DataView(e.memory.buffer,e.vibe_wasm_midi_out());const w=b.getUint32(0,true),r=b.getUint32(4,true);for(let i=r;i<w;i++){const p=16+(i%256)*6;msgs.push({t,st:b.getUint8(p+2),note:b.getUint8(p+3),vel:b.getUint8(p+4)});}b.setUint32(4,w,true);}
 function run(seconds,consume=true){for(let n=0;n<seconds*192000;n+=128){e.vibe_wasm_audio(128,0);t+=128/192000;if(consume)drain();}}
 const press=id=>{ev(3,id,1);ev(3,id,0);};
 const page=n=>{press(19);ev(4,0,0,-60);ev(4,0,0,n);press(9);};
 function held(){const h=new Set();for(const m of msgs){const k=(m.st&15)+':'+m.note;if((m.st&240)===144&&m.vel)h.add(k);else if((m.st&240)===128||((m.st&240)===144&&!m.vel))h.delete(k);}return [...h];}
 set('notes_out',1);set('wander',0);
 return {e,ev,set,run,press,page,msgs,held,drain,now:()=>t};
}

function noDuplicateOnOrOrphanOff(s){const held=new Set();for(const m of s.msgs){const k=(m.st&15)+':'+m.note;if((m.st&240)===144&&m.vel){assert(!held.has(k),'duplicate Note On '+k);held.add(k);}else if((m.st&240)===128||((m.st&240)===144&&!m.vel)){assert(held.delete(k),'orphan Note Off '+k);}}}
{const s=session();s.ev(8,0,1);s.run(90,false);s.set('notes_out',0);s.run(.01,false);s.drain();s.run(.1);check('disabled output retries releases after blocked host resumes',s.held().length===0,s.held());s.ev(8,0,0);s.run(.01);s.set('notes_out',1);s.run(.1);check('re-enable while stopped does not replay unseen events',s.held().length===0,s.held());noDuplicateOnOrOrphanOff(s);}
{const s=session();s.ev(8,0,1);s.run(90,false);new Uint8Array(s.e.memory.buffer,s.e.vibe_wasm_editor_inbox(),3).set([21,64,0]);s.e.vibe_wasm_editor_data_in(3);s.run(.01,false);s.drain();s.run(.1);check('Blank scene retries releases after blocked host resumes',s.held().length===0,s.held());noDuplicateOnOrOrphanOff(s);}
{const s=session();s.ev(8,0,1);s.run(90,false);s.page(2);s.ev(4,3,0,1);s.run(.01,false);s.drain();s.run(.1);check('mute retries release after blocked host resumes',!s.held().some(k=>k.startsWith('0:')),s.held());s.ev(8,0,0);s.run(.1);check('Stop after blocked mute leaves all channels released',s.held().length===0,s.held());noDuplicateOnOrOrphanOff(s);}
result.pass=result.checks.every(x=>x.pass);fs.writeFileSync(process.argv[3],JSON.stringify(result,null,2)+'\n');assert(result.pass);
