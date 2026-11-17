import fs from 'node:fs';
import zlib from 'node:zlib';
import {instantiate} from '../test_host.mjs';
export const wasmPath = process.argv[2] || 'port/build/mutable-dreams-garden.wasm';
export const module = new WebAssembly.Module(fs.readFileSync(wasmPath));
export function session(rate=48000, files=new Map(), media=null) {
  const e=instantiate(module,files,null,media).exports; e.vibe_wasm_init(rate);
  let frame=0;
  const s={e,rate,files,
    event:(type,id,down=0,value=0,midi=0)=>e.vibe_wasm_event(type,id,down,value,midi),
    press:id=>{s.event(3,id,1);s.event(3,id,0);},
    turn:(id,value)=>s.event(4,id,0,value),
    open:id=>{s.press(19);s.turn(0,-60);s.turn(0,id);s.press(9);},
    cmd:(...b)=>{new Uint8Array(e.memory.buffer,e.vibe_wasm_editor_inbox(),b.length).set(b);e.vibe_wasm_editor_data_in(b.length);},
    param:(id)=>e.vibe_wasm_param_value(id),
    set:(id,v)=>e.vibe_wasm_param_set(id,v),
    rear:()=>new DataView(e.memory.buffer,e.vibe_wasm_rear()),
    run:(seconds,source=null,block=128,collect=false)=>{
      const nframes=Math.round(seconds*rate);const out=[];
      for(let k=0;k<nframes;k+=block){
        const n=Math.min(block,nframes-k),io=new Float32Array(e.memory.buffer,e.vibe_wasm_io(),1024);
        if(source) for(let i=0;i<n;i++){const v=source((frame+i)/rate);io[i]=Array.isArray(v)?v[0]:v;io[768+i]=Array.isArray(v)?v[1]:v;}
        e.vibe_wasm_audio(n,source?3:0);
        if(collect) for(let i=0;i<n;i++)out.push(io[256+i]);
        frame+=n;
      }return out;
    },
    status:()=>{
      // Discard the oldest packet, then draw until a fresh status is posted.
      const box=e.vibe_wasm_editor_outbox(); new DataView(e.memory.buffer).setUint32(box,0,true);
      for(let j=0;j<12;j++){
        e.vibe_wasm_draw();const d=new DataView(e.memory.buffer),n=d.getUint32(box,true);if(!n)continue;
        const b=new Uint8Array(e.memory.buffer,box+4,n).slice();d.setUint32(box,0,true);if(b[0]!==100)continue;
        let k=32;const text=[];for(let t=0;t<10;t++){text.push(Buffer.from(b.subarray(k+1,k+1+b[k])).toString());k+=1+b[k];}
        return {playing:b[1],page:text[3],text,flags:b[k+1],input:b[k+6],sing:b[k+7],heard:b[k+9]-1,answer:b[k+10]-1,tour:b[k+11]-1};
      }throw Error('No status packet');
    },
    save:()=>s.event(18,0),
    draw:()=>new Uint16Array(e.memory.buffer,e.vibe_wasm_draw(),480*320).slice(),
    stage:()=>new Uint16Array(e.memory.buffer,e.vibe_wasm_stage_draw(),1280*720).slice(),
    click:(x,y)=>{const v=Math.ceil(y*4/3)*4096+Math.ceil(x*4/3);s.event(19,0,1,v);s.event(19,2,0,v);},
  };return s;
}
export function png(px,w,h,path){
  const raw=Buffer.alloc((w*3+1)*h);
  for(let y=0;y<h;y++)for(let x=0;x<w;x++){const p=px[y*w+x],o=y*(w*3+1)+1+x*3;raw[o]=(p>>11&31)*255/31;raw[o+1]=(p>>5&63)*255/63;raw[o+2]=(p&31)*255/31;}
  const chunk=(t,b)=>{const c=Buffer.alloc(b.length+12);c.writeUInt32BE(b.length);c.write(t,4);b.copy(c,8);c.writeUInt32BE(zlib.crc32(c.subarray(4,8+b.length))>>>0,8+b.length);return c;};
  const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;hdr[9]=2;
  fs.writeFileSync(path,Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]));
}
