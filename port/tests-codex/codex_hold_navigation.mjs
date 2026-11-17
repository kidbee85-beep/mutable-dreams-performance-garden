import fs from 'node:fs';
import {instantiate} from './baseline/test_host.mjs';
const mod=new WebAssembly.Module(fs.readFileSync(process.argv[2]));
function session(){const files=new Map(),e=instantiate(mod,files).exports;e.vibe_wasm_init(48000);const ev=(id,down)=>e.vibe_wasm_event(3,id,down,0,0);const press=id=>{ev(id,1);ev(id,0);};const open=p=>{press(19);e.vibe_wasm_event(4,0,0,-60,0);e.vibe_wasm_event(4,0,0,p,0);press(9);};const read=()=>{e.vibe_wasm_event(18,0,0,0,0);const b=files.get('garden.bin'),v=new DataView(b.buffer,b.byteOffset,b.byteLength);return {mode:v.getInt32(556,true),input:v.getInt32(552,true),sing:v.getInt32(588,true),bytes:b.length};};return{e,ev,press,open,read};}
const results=[];
{const s=session();s.open(26);s.press(1);s.e.vibe_wasm_audio(128,0);const state=s.read();results.push({test:'offset and action sanity: short SING F2 enables FOLLOW',...state,passed:state.input===1&&state.sing===1});}
{const s=session();s.open(25);s.ev(1,1);s.e.vibe_wasm_audio(128,0);s.press(20);s.ev(1,0);s.e.vibe_wasm_audio(128,0);const state=s.read();results.push({test:'F2 started on LISTEN must not activate FOLLOW after RIGHT to SING',...state,passed:state.input===0&&state.sing===0});}
for(const modal of [19,17]){const s=session();s.open(25);s.ev(1,1);s.e.vibe_wasm_audio(128,0);s.press(modal);s.press(modal);s.ev(1,0);s.e.vibe_wasm_audio(128,0);const state=s.read();results.push({test:'pending F2 cancelled by modal open/close '+modal,...state,passed:state.input===0&&state.sing===0&&state.mode===0});}
console.log(JSON.stringify({results},null,2));process.exitCode=results.every(r=>r.passed)?0:1;
