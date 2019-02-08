
# Manbow-J Disassembler for Z80

`mdz80` is Z80 disassembler.
This software was rebuilt for Z80 based on Minachun's 6502 disassembler.

## Usage

> mdZ80.exe [input file] [output file] [options]

**input file**

input filename want to disassemble.

**output file**

output filename.if don't set , output stream is stdout.

**options**

    -s????...Start Address of disassemble. Default is the value
                   which the last addr. is 0xffff.
    -l????...Byte length of disassemble. Default reads until the
             whole input file is disassembled.
    -o????...Start offset Addr. of disassemble. Default is Zero ;
             the head of the input file.
    -m????...Mapper number.If set, output uniq comment of each mapper.
             Default is off. Multiple # is O.K. until 8 times.
             Ex) -m0 ... Sega Master System
                 -m1 ... MSX
                 -m2 ... NEC PC-8801
                 -m3 ... Coleco Vision
    -ni   ...Output numerical value by Intel form.(0nnnnH).
    -nm   ...Output numerical value by Motorola form.($nnnn).
    -nc   ...Output numerical value by C-Languege form.(0xnnnn).
    -r    ...treated as mere null data when there is continuously RST 38H.

Parameter numbers are all hexadecimal.


## Support Mapper

| Mapper No|Type|
----|---- 
| 0 |Sega MkIII/MasterSystem/Game Gear|
| 1 |MSX|
| 2 |PC-8801mkIISR|
| 3 |Coleco Vision|

## Authors

* Original Programd by Minachun
* Reprogrammed by Manbow-J
* Modified by RuRuRu


## License

Compliant with the document of md6502.

	------------------------------------------------------------------------
	全くもって自由ですが、私 Minachun に不利益が出ないようにお願いします。
	再配布も自由ですが、実費以外の金銭の授受がないようにお願いします。
	ソースコードの改変および改変したものをコンパイルした実行ファイルの配布
	は自由に扱ってもらっても構いませんが、私に悪影響が出ない範囲でお願いし
	ます。よりよい改変をなさった方は、私宛に改変内容を御一報くださると非常に
	助かります。
	------------------------------------------------------------------------

