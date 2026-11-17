import assert from 'node:assert/strict';
import fs from 'node:fs';
import {session} from './review-029-host.mjs';
// Synchronous in-memory implementation of the three media operations REC uses.
// No files are sent to the actual host, and no outside SDK dependency is needed.
const results=[];
for(const rate of [44100,48000,96000,192000]){
  const chunks=[];
  const media={store:(op,buf,np,nl,dp,len)=>{
    if(op===6)return 0;
    if(op===10){new Uint8Array(buf,dp,len).set(Buffer.from('sd/local-review.wav\0'));return 1;}
    if(op===11){chunks.push(Buffer.from(new Uint8Array(buf,dp,len)));return 0;}
    if(op===12)return 0;
    return -1;
  }};
  const s=session(rate,new Map(),media);s.press(17);s.press(17);s.open(25);
  for(const k of [1,2,3])s.turn(k,-100);s.press(0);
  const src=t=>.35*Math.sin(2*Math.PI*220*t);s.run(.5,src);s.press(21);
  const out=s.run(.5,src,128,true);s.press(21);s.draw();
  const wav=Buffer.concat(chunks);assert.equal(wav.toString('ascii',0,4),'RIFF');
  let power=0;for(let i=44;i<wav.length;i+=2)power+=(wav.readInt16LE(i)/32768)**2;
  const liveRms=Math.sqrt(out.reduce((a,v)=>a+v*v,0)/out.length),takeRms=Math.sqrt(power/((wav.length-44)/2));
  const row={rate,liveRms,takeRms,wavBytes:wav.length,pass:takeRms>liveRms*.9};results.push(row);console.log(row);
}
fs.writeFileSync(process.argv[3] || '/tmp/review-029-record.json',JSON.stringify(results,null,2));
assert(results.every(r=>r.pass),'REC must include the dry input audible at the outputs');
