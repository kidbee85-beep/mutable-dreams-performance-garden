import fs from 'node:fs';
import assert from 'node:assert/strict';
import {instantiate} from './test_host.mjs';
const wasm=new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const saved=new Map();const original=instantiate(wasm,saved).exports;original.vibe_wasm_init(48000);original.vibe_wasm_event(18,0,1,0,0);assert(saved.has('garden.bin'));
const originalBytes=saved.get('garden.bin');let failures=[];const summaries=[];
for(const offset of [20,24,28])for(const [name,bits] of [['NaN',0x7fc00000],['+Infinity',0x7f800000],['-Infinity',0xff800000]]){
 const store=new Map([...saved].map(([k,v])=>[k,new Uint8Array(v)]));const bytes=store.get('garden.bin');new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength).setUint32(offset,bits,true);
 const e=instantiate(wasm,store).exports;let nonfinite=0,peak=0;
 try{e.vibe_wasm_init(48000);const io=new Float32Array(e.memory.buffer,e.vibe_wasm_io(),4*256);e.vibe_wasm_event(8,0,1,0,0);e.vibe_wasm_event(1,4,1,.8,0);
 for(let block=0;block<1200;block++){if(block===400)e.vibe_wasm_event(1,4,0,0,0);const n=[1,17,73,128,256][block%5];e.vibe_wasm_audio(n,0);for(let i=0;i<n;i++)for(const sample of [io[256+i],io[512+i]]){if(!Number.isFinite(sample))nonfinite++;else peak=Math.max(peak,Math.abs(sample));}}
 const values=[];for(let i=0;i<e.vibe_wasm_param_count();i++)if(!Number.isFinite(e.vibe_wasm_param_value(i)))values.push(i);
 summaries.push({offset,name,nonfinite,peak,nonfiniteParameters:values});if(nonfinite||values.length)failures.push({offset,name,nonfinite,values});
 }catch(err){failures.push({offset,name,trap:String(err)});}
}
console.log(JSON.stringify({baselineStateBytes:originalBytes.length,tests:summaries.length,failures,summaries},null,2));process.exitCode=failures.length?1:0;
