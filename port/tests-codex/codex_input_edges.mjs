import fs from 'node:fs';import {instantiate} from './baseline/test_host.mjs';
const module=new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const results=[];
for(const rate of [44100,48000,96000,192000]){
 const e=instantiate(module).exports;e.vibe_wasm_init(rate);const io=new Float32Array(e.memory.buffer,e.vibe_wasm_io(),1024);
 const press=id=>{e.vibe_wasm_event(3,id,1,0,0);e.vibe_wasm_event(3,id,0,0,0);};press(19);e.vibe_wasm_event(4,0,0,-60,0);e.vibe_wasm_event(4,0,0,25,0);press(9);
 // Default THRU=100. Disable sends to isolate a legitimate direct monitored input plus the garden.
 for(const knob of [1,2,3])e.vibe_wasm_event(4,knob,0,-100,0);
 press(0);e.vibe_wasm_event(8,0,1,0,0);let peak=0,over=0,nonfinite=0;const sizes=[1,17,73,128,256];
 for(let block=0,frame=0;frame<rate*3;block++){
  const n=Math.min(sizes[block%sizes.length],rate*3-frame);io.fill(.98,0,n);io.fill(.98,768,768+n);e.vibe_wasm_audio(n,3);
  for(let i=0;i<n;i++)for(const x of [io[256+i],io[512+i]]){if(!Number.isFinite(x))nonfinite++;else{peak=Math.max(peak,Math.abs(x));if(Math.abs(x)>1)over++;}}
  frame+=n;
 }
 results.push({rate,peak,overRangeSamples:over,nonfinite});
}
console.log(JSON.stringify({scenario:'High but normalized monitored input plus playing garden, no input sends',results},null,2));process.exitCode=results.some(x=>x.overRangeSamples||x.nonfinite)?1:0;
