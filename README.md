# mono-to-stereo

Takes a mono input and renders it as if it was an interleaved stereo input. Works on MS2109 capture
devices where the audio input is a 96khz mono stream but in actuality is a 48khz stereo stream with
the first left channel sample missing. In order to support this device better, accounting for this
missing first sample is done by default.

Original code based off of [Matthew van Eerde's loopback-capture](https://github.com/mvaneerde/blog/tree/master/loopback-capture)
project.

Run `mono-to-stereo.exe -?` for usage instructions.

---
### 対応デバイス
* Macro Silicon MS2109を使用した USB 2.0/ USB 3.0 HDMIキャプチャ
* サウンドハウス CLASSIC PRO CHD201 HDMIビデオキャプチャー
* GENKI ShadowCast

中身は全部 Macro Silicon MS2109

---
# 0.52jp mono-to-stereo FREE WING改造版
for Windows Japanese Language Shift-JIS Localize  
  
デフォルトの出力デバイス名を  
"CABLE Input (VB-Audio Virtual Cable)"  
にした。  
存在しない場合はデフォルトを出力デバイスにします。  
これで、面倒な引数の指定が一切不要になりました。  

また、引数指定の指定子を  
-l  
-i  
-o  
-b  
も受け付ける様にした。  
  
---
# 0.51jp mono-to-stereo FREE WING改造版
for Windows Japanese Language Shift-JIS Localize  
  
日本語 Windowsで --in-deviceの引数指定を無しで動く様にデフォルトの入力デバイス名を  
"デジタル オーディオ インターフェイス"  
に変更し、省略時のデバイス名の比較を前方一致にした。  
※ 正確には部分一致  

前方一致にする事で USB 2.0と USB 3.0のキャプチャデバイスの両方に対応します。  
例：  
"デジタル オーディオ インターフェイス (USB Digital Audio)"  
"デジタル オーディオ インターフェイス (USB3. 0 capture)"  

また、同一デバイスの場合でも USBの差し込みを変更した場合に数字が付与される場合にも対応します。  
例：  
"デジタル オーディオ インターフェイス (2- USB Digital Audio)"  

http://www.neko.ne.jp/~freewing/hardware/usb_hdmi_video_capture_macrosilicon_ms2109_fix_audio_mono_96khz/  

