import assert from 'node:assert/strict';
import fs from 'node:fs';
import {session} from './review-029-host.mjs';
const s=session();s.press(17);s.press(17);s.open(25);for(let k=0;k<4;k++)s.turn(k,-100);s.cmd(27,3);s.set(1,120);s.set(3,0);s.set(29,1);
const notes=[];const rate=48000,block=128;
for(let pos=0;pos<rate*5;pos+=block){
  const io=new Float32Array(s.e.memory.buffer,s.e.vibe_wasm_io(),1024);
  for(let i=0;i<block;i++){const t=(pos+i)/rate,n=t>=.2&&t<.4?69:t>=.45&&t<1.35?71:-1;io[i]=n<0?0:.35*Math.sin(2*Math.PI*440*2**((n-69)/12)*t);}
  s.e.vibe_wasm_audio(block,1);const d=new DataView(s.e.memory.buffer,s.e.vibe_wasm_midi_out()),w=d.getUint32(0,true),r=d.getUint32(4,true);
  for(let i=r;i<w;i++){const o=16+(i%256)*6;if((d.getUint8(o+2)&0xf0)===0x90&&d.getUint8(o+4))notes.push({t:pos/rate,note:d.getUint8(o+3)});}d.setUint32(4,w,true);
}
const gap=notes.length>=2?notes[1].t-notes[0].t:null;
const result={call:'A4 short (0.2 s), B4 long (0.9 s)',expected:'First reverse answer starts B4, then A-derived final note after 0.5 s',notes,gap,pass:gap!==null&&Math.abs(gap-.5)<.02};
console.log(JSON.stringify(result,null,2));fs.writeFileSync(process.argv[3] || '/tmp/review-029-sing-rhythm.json',JSON.stringify(result,null,2));assert(result.pass,'A reversed answer must reverse the note durations together with their pitches');
