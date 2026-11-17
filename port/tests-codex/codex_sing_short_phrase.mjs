import fs from 'node:fs';import {instantiate} from './baseline/test_host.mjs';
const module=new WebAssembly.Module(fs.readFileSync(process.argv[2]));const results=[];
for(const [rate,block] of [[48000,1],[48000,73],[48000,128],[48000,256],[192000,128]]){
 const e=instantiate(module).exports;e.vibe_wasm_init(rate);const io=new Float32Array(e.memory.buffer,e.vibe_wasm_io(),1024);let pos=0;const notes=[];
 const press=id=>{e.vibe_wasm_event(3,id,1,0,0);e.vibe_wasm_event(3,id,0,0,0);};const page=n=>{press(19);e.vibe_wasm_event(4,0,0,-60,0);e.vibe_wasm_event(4,0,0,n,0);press(9);};
 page(25);for(let knob=0;knob<4;knob++)e.vibe_wasm_event(4,knob,0,-100,0);page(26);press(3);e.vibe_wasm_param_set(1,120);e.vibe_wasm_param_set(29,1);
 const midi=e.vibe_wasm_midi_out();
 while(pos<rate*4){const n=Math.min(block,rate*4-pos);for(let j=0;j<n;j++){const t=(pos+j)/rate;const hz=t>=.2&&t<.29?440:t>=.32&&t<.41?493.8833:0;io[j]=hz?.35*Math.sin(2*Math.PI*hz*t):0;io[768+j]=0;}
  e.vibe_wasm_audio(n,1);const dv=new DataView(e.memory.buffer);const w=dv.getUint32(midi,true);let r=dv.getUint32(midi+4,true);for(;r!==w;r=(r+1)>>>0){const q=midi+16+(r%256)*6,frame=dv.getUint16(q,true),st=dv.getUint8(q+2),note=dv.getUint8(q+3),velocity=dv.getUint8(q+4);if((st&0xf0)===0x90&&velocity>0)notes.push({t:(pos+frame)/rate,note,channel:st&15});}dv.setUint32(midi+4,w,true);pos+=n;
 }
 results.push({rate,block,notes,firstGap:notes.length>=2?notes[1].t-notes[0].t:null});
}
console.log(JSON.stringify({scenario:'ANSWER to the same two short sung notes at 120 BPM, input path otherwise dry-muted',results},null,2));const gaps=results.filter(r=>r.rate===48000).map(r=>r.firstGap).filter(x=>x!==null);process.exitCode=gaps.length<4||Math.max(...gaps)-Math.min(...gaps)>.05?1:0;
