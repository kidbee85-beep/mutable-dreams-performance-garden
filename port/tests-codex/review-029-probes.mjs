import assert from 'node:assert/strict';
import fs from 'node:fs';
import {session,png} from './review-029-host.mjs';
const results=[];
function check(name,fn){try{const detail=fn();results.push({name,pass:true,detail});console.log('PASS',name,JSON.stringify(detail));}catch(e){results.push({name,pass:false,error:e.message});console.log('FAIL',name,e.message);}}
const tone=(note,amp=.35)=>t=>amp*Math.sin(2*Math.PI*440*2**((note-69)/12)*t);
check('CV IN updates a newly selected destination at constant voltage',()=>{
  const s=session();s.press(17);s.press(17);s.open(49);
  s.rear().setFloat32(8,5,true);s.turn(3,1);s.run(.2);
  const density=s.param(4),before=s.param(5);s.turn(3,1);s.run(.2);const motion=s.param(5);
  console.log({density,before,motion});assert.equal(motion,100,'5 V must set MOTION to 100% after DENSITY -> MOTION');return {density,before,motion};
});
check('MOVES respects an encoder change to the resting macro',()=>{
  const s=session();s.press(17);s.press(17);s.open(25);s.press(4);s.run(.3,()=>0);s.open(0);s.turn(0,10);s.run(.05,()=>0);const moved=s.param(4);s.run(.4,()=>0);const settled=s.param(4);
  s.open(25);for(let i=0;i<4;i++)s.press(4);s.run(.2,()=>0);const off=s.param(4);
  console.log({moved,settled,off});assert.equal(off,moved,'turning MOVES off must preserve the new rest value');return {moved,settled,off};
});
check('STEREO MOVES responds to input 2 when input 1 is quiet',()=>{
  const s=session();s.press(17);s.press(17);s.open(25);s.press(2);s.press(4);s.run(.3,()=>[0,0]);const quiet=s.param(4);
  const out=s.run(.8,t=>[0,.5*Math.sin(2*Math.PI*220*t)],128,true),loud=s.param(4);
  s.draw();const d=new DataView(s.e.memory.buffer,s.e.vibe_wasm_io());let rightPeak=0;for(let i=0;i<128;i++)rightPeak=Math.max(rightPeak,Math.abs(d.getFloat32((512+i)*4,true)));
  console.log({quiet,loud,rightPeak});assert(loud>quiet+15,'STEREO input loudness must include IN 2 for MOVES and the input meter');return {quiet,loud,rightPeak};
});
check('KEY waits for new input after input off/on',()=>{
  const s=session();s.press(17);s.press(17);s.cmd(26,1);s.run(.7,tone(62));const heard=s.status().heard;
  s.cmd(26,0);s.run(1);s.set(2,0);s.run(.1);s.open(26);s.press(4);s.run(.04,()=>0);const root=s.param(2);
  console.log({heard,root});assert.equal(root,0,'arming KEY in silence must not reuse the D heard before INPUT OFF');return {heard,root};
});
check('INPUT OFF then ON retriggers the same sung note in FOLLOW',()=>{
  const s=session();s.press(17);s.press(17);s.cmd(27,1);s.run(.7,tone(62));s.cmd(26,0);s.run(1);s.cmd(26,1);
  // Status answer is only an indicator: collect MIDI note-ons as the independent observable.
  s.set(29,1);s.run(.7,tone(62));const d=new DataView(s.e.memory.buffer,s.e.vibe_wasm_midi_out()),w=d.getUint32(0,true),r=d.getUint32(4,true);let ons=0;
  for(let i=r;i<w;i++){const o=16+(i%256)*6;if((d.getUint8(o+2)&0xf0)===0x90&&d.getUint8(o+4))ons++;}
  console.log({ons,status:s.status()});assert(ons>0,'same D after a one-second input-off pause must trigger FOLLOW again');return {ons};
});
check('v10 migration preserves settings, raw gesture bytes and disables tour',()=>{
  const f=new Map(),a=session(48000,f);a.press(17);a.press(17);a.open(25);a.press(0);a.press(1);a.press(2);a.open(26);a.press(3);a.turn(0,2);a.event(1,15,1,.8);a.event(1,15,0);a.event(1,4,1,.8);a.run(.7);a.event(1,4,0);a.event(1,15,1,.8);a.event(1,15,0);a.save();
  const b=f.get('garden.bin'),d=new DataView(b.buffer,b.byteOffset),n=d.getUint32(68,true),old=new Uint8Array(b.length-12);old.set(b.subarray(0,620));old.set(b.subarray(632),620);new DataView(old.buffer).setUint32(4,10,true);f.set('garden.bin',old);
  const z=session(48000,f);z.run(.1);const st=z.status();z.save();const after=f.get('garden.bin');
  assert.equal(st.input,1);assert.equal(st.sing,3);assert.equal(st.tour,-1);assert.equal(z.rear().getFloat32(12,true),1);assert.equal(z.rear().getFloat32(20,true),1);assert.deepEqual(after.subarray(632),b.subarray(632));assert.equal(new DataView(after.buffer).getInt32(620,true),0);return {gestureLength:n,bytes:after.length,status:st};
});
check('six PLACES tiles open their exact destinations',()=>{
  const s=session();s.press(17);s.press(17);const got=[];
  for(let i=0;i<6;i++){s.click(660,26);s.click(728,26);s.click(24+(i%3)*220+80,72+Math.floor(i/3)*238+90);got.push(s.status().page);}
  assert.deepEqual(got,['SPACE','GRAIN','TAPE','TAPE','LISTEN','SING']);return got;
});
check('menu banks reach all 50 pages through editor display clicks',()=>{
  const s=session();s.press(17);s.press(17);const names=[];
  for(let i=0;i<50;i++){s.open(0);s.press(19);if(i>=36)s.cmd(19,120,153);const at=i%36;s.cmd(19,Math.floor((12+Math.floor(at/12)*156+42)/2),Math.floor((60+(at%12)*20+10)/2));names.push(s.status().page);}
  assert.equal(new Set(names).size,50);return names;
});
check('editor 26/27/28 rejects short messages and invalid SING modes',()=>{
  const s=session();s.press(17);s.press(17);s.cmd(26);s.cmd(27);s.cmd(27,255);assert.equal(s.status().input,0);s.cmd(27,2);let x=s.status();assert.equal(x.input,1);assert.equal(x.sing,2);s.cmd(26,0);assert.equal(s.status().input,0);s.cmd(28);assert.equal(s.status().tour,0);s.cmd(28);assert.equal(s.status().tour,-1);return true;
});
check('Scale2x mirror exactly matches the EPX rule including boundaries',()=>{
  const s=session();s.press(17);s.press(17);s.open(6);const dev=s.draw(),stage=s.stage();let mismatch=0;
  for(let y=0;y<320;y++)for(let x=0;x<480;x++){
    const E=dev[y*480+x],B=dev[Math.max(0,y-1)*480+x],H=dev[Math.min(319,y+1)*480+x],D=dev[y*480+Math.max(0,x-1)],F=dev[y*480+Math.min(479,x+1)];
    const expected=B!==H&&D!==F?[D===B?D:E,B===F?F:E,D===H?D:E,H===F?F:E]:[E,E,E,E];
    for(let k=0;k<4;k++)if(stage[(75+y*2+(k>>1))*1280+21+x*2+(k&1)]!==expected[k])mismatch++;
  }assert.equal(mismatch,0);return {pixels:480*320*4,mismatch};
});
check('holding distinct LISTEN keys displays their distinct explanations',()=>{
  const images=[];
  for(const key of [0,1]){const s=session();s.press(17);s.press(17);s.open(25);s.event(3,key,1);s.run(.6);assert(s.status().flags&2);images.push(s.draw());}
  let changed=0;for(let i=0;i<images[0].length;i++)changed+=images[0][i]!==images[1][i];
  console.log({changed});assert(changed>0,'F1 INPUT and F2 LINE/MIC/HI-Z should show different explanations; say() is hidden by draw_help()');return {changed};
});
// Small reproducible pictures for manual visual QA; source code remains untouched.
for(const [name,setup]of [
  ['help-listen',s=>{s.open(25);s.event(3,1,1);s.run(.6);}],
  ['menu-bank1',s=>s.press(19)],['menu-bank2',s=>{s.press(19);s.turn(0,36);}],
  ['cloud',s=>s.open(23)],['tour-stage',s=>{s.cmd(28);}],
]){const s=session();s.press(17);s.press(17);setup(s);png(s.draw(),480,320,`/tmp/review-029-${name}.png`);if(name==='tour-stage')png(s.stage(),1280,720,'/tmp/review-029-tour-stage-large.png');}
fs.writeFileSync(process.argv[3] || '/tmp/review-029-probes.json',JSON.stringify(results,null,2));process.exitCode=results.some(r=>!r.pass)?1:0;
