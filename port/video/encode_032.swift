// Mutable Dreams · Performance Garden – a video straight from the firmware (0.30), by Kid Bunka.
// stdin: raw RGB565 frames (w x h, little endian). Writes an H.264 .mp4; then muxes a WAV as AAC.
// usage: swift encode.swift <w> <h> <fps> <audio.wav> <out.mp4> [captions.tsv: start<TAB>end<TAB>text per line] [band px]
// (0.30) band: the captions get their own dark band below the picture, so they never cover a control
import AVFoundation
import CoreVideo
import CoreText
import AppKit

let a = CommandLine.arguments
let iw = Int(a[1])!, ih = Int(a[2])!, fps = Int32(a[3])!
let portrait = a.count > 7 && a[7] == "portrait"
let w = portrait ? 1080 : 1920
let band = 0, h = portrait ? 1920 : 1080
let consoleImages:[CGImage] = a.dropFirst(8).compactMap { path in let im=NSImage(contentsOfFile:path);return im?.cgImage(forProposedRect:nil,context:nil,hints:nil) }
let wavURL = URL(fileURLWithPath: a[4]), outURL = URL(fileURLWithPath: a[5])
let tmpURL = outURL.deletingPathExtension().appendingPathExtension("video-only.mp4")
// captions: a calm line at the bottom, fading in and out
var captions: [(Double, Double, String)] = []
if a.count > 6, let txt = try? String(contentsOfFile: a[6], encoding: .utf8) {
    for line in txt.split(separator: "\n") { let f = line.split(separator: "\t", maxSplits: 2); if f.count == 3, let s0 = Double(f[0]), let s1 = Double(f[1]) { captions.append((s0, s1, String(f[2]))) } }
}
func line(_ ctx: CGContext, _ text: String, _ x: CGFloat, _ y: CGFloat, _ size: CGFloat, _ color: NSColor, _ maxWidth: CGFloat, bold: Bool = false) {
 var fontSize=size
 while fontSize>16 {
  let f=CTFontCreateWithName((bold ? "Avenir Next Demi Bold":"Avenir Next Regular") as CFString,fontSize,nil)
  let attrs:[NSAttributedString.Key:Any]=[.font:f,.foregroundColor:color]
  let l=CTLineCreateWithAttributedString(NSAttributedString(string:text,attributes:attrs))
  let bounds=CTLineGetBoundsWithOptions(l,.useGlyphPathBounds)
  if bounds.width<=maxWidth || fontSize<=17 {ctx.textPosition=CGPoint(x:x,y:y);CTLineDraw(l,ctx);return};fontSize-=1
 }
}
func drawCaption(_ ctx: CGContext, _ t: Double) {
 let white=NSColor(calibratedRed:0.9,green:0.97,blue:0.93,alpha:1), gold=NSColor(calibratedRed:0.94,green:0.76,blue:0.55,alpha:1)
 if portrait {
  line(ctx,"MUTABLE DREAMS",74,1690,66,white,932,bold:true)
  line(ctx,"PERFORMANCE GARDEN 0.32",78,1623,31,gold,928,bold:true)
  line(ctx,"A garden you can play.",78,1548,46,white,928)
  line(ctx,"ORIGINAL FIRMWARE AUDIO / NO NARRATION",78,290,23,gold,930)
  line(ctx,"Kid Bunka  /  @mutabledreams",78,225,34,white,930,bold:true)
  line(ctx,"Full tour, download and guide: link in bio",78,165,28,white,930)
 }
 guard let c=captions.first(where:{t >= $0.0 && t < $0.1}) else{return}
 let fade=max(0,min(1,(t-c.0)/0.3,(c.1-t)/0.3)), parts=c.2.components(separatedBy:" | "),kicker=parts.count>1 ? parts[0] : "",text=parts.count>1 ? parts[1] : c.2
 let col=NSColor(calibratedRed:0.9,green:0.97,blue:0.93,alpha:fade), accent=NSColor(calibratedRed:0.94,green:0.76,blue:0.55,alpha:fade)
 if !portrait {line(ctx,kicker.uppercased(),128,101,22,accent,1664,bold:true);line(ctx,text,128,44,38,col,1664)}
 else {
  line(ctx,kicker.uppercased(),78,643,36,accent,928,bold:true)
  let words=text.split(separator:" ");var rows=[String](),row=""
  for word in words {let trial=row.isEmpty ? String(word):row+" "+word;if trial.count>40 {rows.append(row);row=String(word)}else{row=trial}}
  if !row.isEmpty{rows.append(row)}
  for(i,r)in rows.prefix(4).enumerated(){line(ctx,r,78,566-CGFloat(i)*61,44,col,930)}
 }
}
try? FileManager.default.removeItem(at: tmpURL); try? FileManager.default.removeItem(at: outURL)

let writer = try! AVAssetWriter(outputURL: tmpURL, fileType: .mp4)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264, AVVideoWidthKey: w, AVVideoHeightKey: h,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: portrait ? 12_000_000 : 14_000_000, AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel]])
input.expectsMediaDataInRealTime = false
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA, kCVPixelBufferWidthKey as String: w, kCVPixelBufferHeightKey as String: h])
writer.add(input)
writer.startWriting(); writer.startSession(atSourceTime: .zero)

let frameBytes = iw * ih * 2
var frame = 0
let stdin = FileHandle.standardInput
var buf = Data()
while true {
    while buf.count < frameBytes { let d = stdin.readData(ofLength: frameBytes - buf.count); if d.isEmpty { break }; buf.append(d) }
    if buf.count < frameBytes { break }
    var pb: CVPixelBuffer?
    CVPixelBufferPoolCreatePixelBuffer(nil, adaptor.pixelBufferPool!, &pb)
    guard let px = pb else { break }
    CVPixelBufferLockBaseAddress(px, [])
    let base=CVPixelBufferGetBaseAddress(px)!, stride=CVPixelBufferGetBytesPerRow(px)
    let ctx=CGContext(data:base,width:w,height:h,bitsPerComponent:8,bytesPerRow:stride,space:CGColorSpaceCreateDeviceRGB(),bitmapInfo:CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue)!
    ctx.setFillColor(CGColor(red:0.025,green:0.07,blue:0.06,alpha:1));ctx.fill(CGRect(x:0,y:0,width:w,height:h))
    // Slow plant-like border lines, drawn only in the video layout.
    ctx.setStrokeColor(CGColor(red:0.2,green:0.44,blue:0.34,alpha:0.22));ctx.setLineWidth(2)
    for k in 0..<9 {let x=CGFloat(k)*150+30,tt=CGFloat(frame)/CGFloat(fps);ctx.move(to:CGPoint(x:x,y:0));ctx.addQuadCurve(to:CGPoint(x:x+45*sin(tt/9+CGFloat(k)),y:portrait ? 1500:1080),control:CGPoint(x:x-70,y:600));ctx.strokePath()}
    var rgba=Data(count:iw*ih*4)
    rgba.withUnsafeMutableBytes { target in buf.withUnsafeBytes { source in
      let d=target.bindMemory(to:UInt8.self),s=source.bindMemory(to:UInt16.self)
      for i in 0..<(iw*ih){let value=s[i],j=i*4;d[j]=UInt8((Int(value)&31)*255/31);d[j+1]=UInt8((Int(value>>5)&63)*255/63);d[j+2]=UInt8((Int(value>>11)&31)*255/31);d[j+3]=255}
    }}
    let provider=CGDataProvider(data:rgba as CFData)!
    let image=CGImage(width:iw,height:ih,bitsPerComponent:8,bitsPerPixel:32,bytesPerRow:iw*4,space:CGColorSpaceCreateDeviceRGB(),bitmapInfo:CGBitmapInfo(rawValue:CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue),provider:provider,decode:nil,shouldInterpolate:true,intent:.defaultIntent)!
    ctx.interpolationQuality = .high
    let rect=portrait ? CGRect(x:0,y:770,width:1080,height:607.5):CGRect(x:128,y:144,width:1664,height:936)
    let now=Double(frame)/Double(fps)
    if !portrait && consoleImages.count>=3 && now>=302 && now<320 {
      let chosen=consoleImages[now<307 ? 0 : (now<312 ? 1:2)]
      let scale=min(rect.width/CGFloat(chosen.width),rect.height/CGFloat(chosen.height)),cw=CGFloat(chosen.width)*scale,ch=CGFloat(chosen.height)*scale
      ctx.draw(chosen,in:CGRect(x:rect.midX-cw/2,y:rect.midY-ch/2,width:cw,height:ch))
    }else{ctx.draw(image,in:rect)}
    ctx.setStrokeColor(CGColor(red:0.45,green:0.72,blue:0.58,alpha:0.55));ctx.setLineWidth(2);ctx.stroke(rect.insetBy(dx:1,dy:1))
    drawCaption(ctx,Double(frame)/Double(fps))
    CVPixelBufferUnlockBaseAddress(px, [])
    while !input.isReadyForMoreMediaData { usleep(1000) }
    if !adaptor.append(px, withPresentationTime: CMTime(value: CMTimeValue(frame), timescale: fps)) { fatalError("Video append failed: \(String(describing:writer.error))") }
    frame += 1
    buf.removeAll(keepingCapacity: true)
}
input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()
FileHandle.standardError.write("video frames: \(frame)\n".data(using: .utf8)!)

// Encode the exact firmware WAV as stereo AAC 320 kbit/s, then mux without re-encoding the video.
let sem = DispatchSemaphore(value:0)
Task {
 defer { sem.signal() }
 do {
  guard writer.status == .completed else { throw writer.error ?? NSError(domain:"VideoWriter",code:1) }
  let aa=AVURLAsset(url:wavURL),track=try await aa.loadTracks(withMediaType:.audio).first!
  let reader=try AVAssetReader(asset:aa)
  let output=AVAssetReaderTrackOutput(track:track,outputSettings:[AVFormatIDKey:kAudioFormatLinearPCM,AVLinearPCMBitDepthKey:16,AVLinearPCMIsFloatKey:false,AVLinearPCMIsNonInterleaved:false])
  reader.add(output)
  let encodedAudio=outURL.deletingPathExtension().appendingPathExtension("audio-only.m4a");try? FileManager.default.removeItem(at:encodedAudio)
  let aw=try AVAssetWriter(outputURL:encodedAudio,fileType:.m4a)
  let ai=AVAssetWriterInput(mediaType:.audio,outputSettings:[AVFormatIDKey:kAudioFormatMPEG4AAC,AVSampleRateKey:44100,AVNumberOfChannelsKey:2,AVEncoderBitRateKey:320000]);ai.expectsMediaDataInRealTime=false;aw.add(ai)
  guard aw.startWriting(),reader.startReading() else{throw NSError(domain:"AudioStart",code:1)};aw.startSession(atSourceTime:.zero)
  while let sample=output.copyNextSampleBuffer(){while !ai.isReadyForMoreMediaData{try await Task.sleep(for:.milliseconds(1))};if !ai.append(sample){throw aw.error ?? NSError(domain:"AudioAppend",code:1)}}
  ai.markAsFinished();await aw.finishWriting();guard aw.status == .completed && reader.status == .completed else{throw aw.error ?? reader.error ?? NSError(domain:"AudioFinish",code:1)}
  let comp=AVMutableComposition(),va=AVURLAsset(url:tmpURL),ea=AVURLAsset(url:encodedAudio)
  let vt=try await va.loadTracks(withMediaType:.video).first!,at=try await ea.loadTracks(withMediaType:.audio).first!,vd=try await va.load(.duration)
  let cv=comp.addMutableTrack(withMediaType:.video,preferredTrackID:kCMPersistentTrackID_Invalid)!,ca=comp.addMutableTrack(withMediaType:.audio,preferredTrackID:kCMPersistentTrackID_Invalid)!
  try cv.insertTimeRange(CMTimeRange(start:.zero,duration:vd),of:vt,at:.zero);try ca.insertTimeRange(CMTimeRange(start:.zero,duration:vd),of:at,at:.zero)
  let ex=AVAssetExportSession(asset:comp,presetName:AVAssetExportPresetPassthrough)!;ex.shouldOptimizeForNetworkUse=true;try await ex.export(to:outURL,as:.mp4)
  try? FileManager.default.removeItem(at:encodedAudio);try? FileManager.default.removeItem(at:tmpURL)
  FileHandle.standardError.write("written: \(outURL.path), stereo AAC 320 kbit/s\n".data(using:.utf8)!)
 }catch{FileHandle.standardError.write("EXPORT FAILED: \(error)\n".data(using:.utf8)!);exit(1)}
}
sem.wait()
