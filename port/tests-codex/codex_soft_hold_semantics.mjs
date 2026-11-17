import fs from 'node:fs';import {instantiate} from './baseline/test_host.mjs';
const module=new WebAssembly.Module(fs.readFileSync(process.argv[2]));const results=[];
for(const spec of [{page:25,key:0,long:false,draw:true},{page:25,key:0,long:true,draw:true},{page:25,key:0,long:true,draw:false},{page:26,key:1,long:true,draw:false},{page:39,key:0,long:true,draw:true},{page:44,key:0,long:true,draw:true}]){
 const files=new Map(),e=instantiate(module,files).exports;e.vibe_wasm_init(48000);
 const press=id=>{e.vibe_wasm_event(3,id,1,0,0);e.vibe_wasm_event(3,id,0,0,0);};
 press(19);e.vibe_wasm_event(4,0,0,-60,0);e.vibe_wasm_event(4,0,0,spec.page,0);press(9);
 e.vibe_wasm_event(3,spec.key,1,0,0);for(let f=0;f<(spec.long?.9:0)*48000;f+=128){e.vibe_wasm_audio(128,0);if(spec.draw)e.vibe_wasm_draw();}
 e.vibe_wasm_event(3,spec.key,0,0,0);const bankWrites=[...files.keys()].filter(n=>n.startsWith('banks/'));
 e.vibe_wasm_event(18,0,1,0,0);const state=files.get('garden.bin'),dv=new DataView(state.buffer,state.byteOffset,state.byteLength);const input=dv.getInt32(552,true),sing=dv.getInt32(588,true),radio=dv.getInt32(388,true);
 const result={...spec,savedInputOn:input,bankWrites,savedBytes:state.length,radioGuess:radio};
 if(spec.page===25)result.passed=input===(spec.long?0:1);else if(spec.page===26)result.passed=input===0&&sing===0;else if(spec.page===44)result.passed=bankWrites.length===0;else{
  // Status prefix byte2 is ON AIR; wait until a current response is available.
  let onAir=null;for(let k=0;k<100;k++){e.vibe_wasm_audio(128,0);e.vibe_wasm_draw();const p=e.vibe_wasm_editor_outbox(),d=new DataView(e.memory.buffer),len=d.getUint32(p,true);if(!len)continue;const b=new Uint8Array(e.memory.buffer,p+4,len).slice();d.setUint32(p,0,true);if(b[0]===100){onAir=b[2];break;}}
  result.onAir=onAir;result.passed=onAir===0;
 }
 results.push(result);
}
console.log(JSON.stringify({scope:'hold suppresses short action even with editor closed; help visibility checked separately',results},null,2));process.exitCode=results.every(r=>r.passed)?0:1;
