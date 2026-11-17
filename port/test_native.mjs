import assert from "node:assert/strict";
import {instantiate} from "./test_host.mjs";
import fs from "node:fs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const results = [];
for (const rate of [44100,48000,96000,192000]) {
  function session(pressure) {
    const {exports:e} = instantiate(module);
    e.vibe_wasm_init(rate);
    const io = new Float32Array(e.memory.buffer,e.vibe_wasm_io(),3*256);
    const send=(type,id,down,value,midi=0)=>e.vibe_wasm_event(type,id,down,value,midi);
    const sizes=[1,17,73,128,256,55];
    const data=[];
    let position=0, peak=0, power=0, stage=0, clicks=0;
    send(9,57,1,.81,3<<8);
    const before = fsHash(e);
    for (let block=0;position<rate*1.7;block++) {
      const n=sizes[block%sizes.length];
      if (stage===0&&position>rate*.10) {
        if (pressure!==null) send(1,8,1,.68);
        stage++;
      }
      if (stage===1&&position>rate*.18) {
        if (pressure!==null) send(5,0,0,0,0xa9|(44<<8)|(pressure<<16));
        stage++;
      }
      if (stage===2&&position>rate*.71) {
        if (pressure!==null) send(1,8,0,0);
        send(9,57,0,0,3<<8);
        stage++;
      }
      e.vibe_wasm_audio(n,0);
      for(let i=0;i<n;i++) {
        const l=io[256+i], r=io[512+i];
        assert(Number.isFinite(l)&&Number.isFinite(r));
        peak=Math.max(peak,Math.abs(l),Math.abs(r));
        power+=l*l+r*r;
        if(data.length) clicks=Math.max(clicks,Math.abs(l-data.at(-1)));
        data.push(l);
      }
      position+=n;
    }
    send(3,1,1,0);
    e.vibe_wasm_audio(128,0);
    const other = fsHash(e);
    assert.notEqual(before,other);
    assert(peak>.001&&peak<1&&power>.0001);
    return {data,peak,clicks};
  }
  const dry=session(null), soft=session(8), deep=session(127);
  const difference=deep.data.reduce((sum,x,i)=>sum+Math.abs(x-dry.data[i]),0);
  const pressure=deep.data.reduce((sum,x,i)=>sum+Math.abs(x-soft.data[i]),0);
  assert(difference>.02 && pressure>.01,JSON.stringify({rate,difference,pressure}));
  results.push({rate, stereoPeak:deep.peak, maxSampleStep:deep.clicks,
    roomDifference:difference, pressureDifference:pressure});
}
function fsHash(e) {
  const frame=new Uint16Array(e.memory.buffer,e.vibe_wasm_draw(),480*320);
  let hash=2166136261;
  for(const pixel of frame) hash=Math.imul(hash^pixel,16777619);
  return hash>>>0;
}
console.log(JSON.stringify(results,null,2));
