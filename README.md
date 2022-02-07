# Lovyan GFX for Discovery kit with STM32F746NG
## STM32F746G-DISCO にて [Lovyan GFX](https://github.com/lovyan03/LovyanGFX) を動かすコード
### 動かすに必要なもの
- Arduino 開発環境
- stm32duino ライブラリ \
    導入方法は [Getting start](https://github.com/stm32duino/Arduino_Core_STM32#getting-started) を参照
- [Lovyan GFX](https://github.com/lovyan03/LovyanGFX) ライブラリ \
    Arduino IDEの`ツール` → `ライブラリを管理...` から `LovyanGFX` と検索するとインストールできる。

## 仕様
- Lovyan GFXのOpenCV対応コードから移植
- Lovyan GFXのタッチパネルI/Fは未対応
- DMA未使用
- シングルバッファリング
- カラーモードは`RGB565`の16bit
- SDRAMを使用(`0xC0000000`から8MiB分まで)
- フレームバッファに`0xC0000000`から`261120 bytes`(480x272x2)を使用
