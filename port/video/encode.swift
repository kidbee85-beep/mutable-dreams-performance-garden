// Mutable Dreams · Performance Garden – a video straight from the firmware (0.30), by Kid Bunka.
// stdin: raw RGB565 frames (w x h, little endian). Writes an H.264 .mp4; then muxes a WAV as AAC.
// usage: swift encode.swift <w> <h> <fps> <audio.wav> <out.mp4> [captions.tsv: start<TAB>end<TAB>text per line] [band px]
// (0.30) band: the captions get their own dark band below the picture, so they never cover a control
import AVFoundation
import CoreVideo
import CoreText
import AppKit

let a = CommandLine.arguments
let w = Int(a[1])!, ih = Int(a[2])!, fps = Int32(a[3])!
let band = a.count > 7 ? Int(a[7])! : 0, h = ih + band
let wavURL = URL(fileURLWithPath: a[4]), outURL = URL(fileURLWithPath: a[5])
let tmpURL = outURL.deletingPathExtension().appendingPathExtension("video-only.mp4")
// captions: a calm line at the bottom, fading in and out
var captions: [(Double, Double, String)] = []
if a.count > 6, let txt = try? String(contentsOfFile: a[6], encoding: .utf8) {
    for line in txt.split(separator: "\n") { let f = line.split(separator: "\t", maxSplits: 2); if f.count == 3, let s0 = Double(f[0]), let s1 = Double(f[1]) { captions.append((s0, s1, String(f[2]))) } }
}
func drawCaption(_ px: CVPixelBuffer, _ t: Double) {
    guard let c = captions.first(where: { t >= $0.0 && t < $0.1 }) else { return }
    let fade = min(1.0, (t - c.0) / 0.4, (c.1 - t) / 0.4)
    let base = CVPixelBufferGetBaseAddress(px)!
    guard let ctx = CGContext(data: base, width: w, height: h, bitsPerComponent: 8, bytesPerRow: CVPixelBufferGetBytesPerRow(px),
                              space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue) else { return }
    // (0.31) "KICKER | text": a small gold chapter name above the line
    let parts = c.2.components(separatedBy: " | ")
    let kicker = parts.count > 1 ? parts[0] : "", text = parts.count > 1 ? parts[1] : c.2
    let font = CTFontCreateWithName("Avenir Next Demi Bold" as CFString, 30, nil)
    let attrs: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: NSColor(calibratedRed: 0.93, green: 0.97, blue: 0.95, alpha: CGFloat(fade))]
    let line = CTLineCreateWithAttributedString(NSAttributedString(string: text, attributes: attrs))
    let bounds = CTLineGetBoundsWithOptions(line, .useGlyphPathBounds)
    let kfont = CTFontCreateWithName("Avenir Next Bold" as CFString, 15, nil)
    let kattrs: [NSAttributedString.Key: Any] = [.font: kfont, .kern: 2.4, .foregroundColor: NSColor(calibratedRed: 0.94, green: 0.76, blue: 0.55, alpha: CGFloat(fade))]
    let kline = CTLineCreateWithAttributedString(NSAttributedString(string: kicker.uppercased(), attributes: kattrs))
    let kb = CTLineGetBoundsWithOptions(kline, .useGlyphPathBounds)
    let hasK = !kicker.isEmpty
    let bw = max(bounds.width, kb.width) + 56, bh: CGFloat = hasK ? 82 : 58, bx = (CGFloat(w) - bw) / 2
    let by: CGFloat = band > 0 ? (CGFloat(band) - bh) / 2 : 36
    ctx.setFillColor(CGColor(red: 0.03, green: 0.06, blue: 0.07, alpha: 0.82 * fade))
    ctx.addPath(CGPath(roundedRect: CGRect(x: bx, y: by, width: bw, height: bh), cornerWidth: 16, cornerHeight: 16, transform: nil)); ctx.fillPath()
    ctx.setFillColor(CGColor(red: 0.94, green: 0.76, blue: 0.55, alpha: 0.9 * fade))
    ctx.fill(CGRect(x: bx + 20, y: by + bh - 3, width: bw - 40, height: 2))
    if hasK {
        ctx.textPosition = CGPoint(x: bx + 28 - kb.minX, y: by + bh - 30 - kb.minY)
        CTLineDraw(kline, ctx)
        ctx.textPosition = CGPoint(x: bx + 28 - bounds.minX, y: by + 14 - bounds.minY)
    } else {
        ctx.textPosition = CGPoint(x: bx + 28 - bounds.minX, y: by + (bh - bounds.height) / 2 - bounds.minY)
    }
    CTLineDraw(line, ctx)
}
try? FileManager.default.removeItem(at: tmpURL); try? FileManager.default.removeItem(at: outURL)

let writer = try! AVAssetWriter(outputURL: tmpURL, fileType: .mp4)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264, AVVideoWidthKey: w, AVVideoHeightKey: h,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: 10_000_000, AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel]])
input.expectsMediaDataInRealTime = false
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA, kCVPixelBufferWidthKey as String: w, kCVPixelBufferHeightKey as String: h])
writer.add(input)
writer.startWriting(); writer.startSession(atSourceTime: .zero)

let frameBytes = w * ih * 2
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
    let dst = CVPixelBufferGetBaseAddress(px)!.assumingMemoryBound(to: UInt8.self), stride = CVPixelBufferGetBytesPerRow(px)
    buf.withUnsafeBytes { (src: UnsafeRawBufferPointer) in
        let s = src.bindMemory(to: UInt16.self)
        for y in 0..<ih { for x in 0..<w {
            let v = s[y * w + x], o = y * stride + x * 4
            let r = UInt8((Int(v >> 11) & 31) * 255 / 31), g = UInt8((Int(v >> 5) & 63) * 255 / 63), b = UInt8((Int(v) & 31) * 255 / 31)
            dst[o] = b; dst[o + 1] = g; dst[o + 2] = r; dst[o + 3] = 255
        } }
    }
    for y in ih..<h { for x in 0..<w { let o = y * stride + x * 4; dst[o] = 0x14; dst[o + 1] = 0x10; dst[o + 2] = 0x0a; dst[o + 3] = 255 } }   /* the band */
    drawCaption(px, Double(frame) / Double(fps))
    CVPixelBufferUnlockBaseAddress(px, [])
    while !input.isReadyForMoreMediaData { usleep(1000) }
    adaptor.append(px, withPresentationTime: CMTime(value: CMTimeValue(frame), timescale: fps))
    frame += 1
    buf.removeAll(keepingCapacity: true)
}
input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()
FileHandle.standardError.write("video frames: \(frame)\n".data(using: .utf8)!)

// mux: the video and the firmware's own sound (AAC)
let comp = AVMutableComposition()
let va = AVURLAsset(url: tmpURL), aa = AVURLAsset(url: wavURL)
let sem = DispatchSemaphore(value: 0)
Task {
    let vt = try await va.loadTracks(withMediaType: .video).first!, at = try await aa.loadTracks(withMediaType: .audio).first!
    let vd = try await va.load(.duration)
    let cv = comp.addMutableTrack(withMediaType: .video, preferredTrackID: kCMPersistentTrackID_Invalid)!
    let ca = comp.addMutableTrack(withMediaType: .audio, preferredTrackID: kCMPersistentTrackID_Invalid)!
    try cv.insertTimeRange(CMTimeRange(start: .zero, duration: vd), of: vt, at: .zero)
    try ca.insertTimeRange(CMTimeRange(start: .zero, duration: vd), of: at, at: .zero)
    let ex = AVAssetExportSession(asset: comp, presetName: AVAssetExportPresetHighestQuality)!
    try await ex.export(to: outURL, as: .mp4)
    FileHandle.standardError.write("written: \(outURL.path)\n".data(using: .utf8)!)
    sem.signal()
}
sem.wait()
try? FileManager.default.removeItem(at: tmpURL)
