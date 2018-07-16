import bincopy

def convBinToSREC(inname, outname):
    print("Converting", inname, "to", outname)
    f = bincopy.BinFile()
    f.add_binary_file(inname)
    with open(outname, "w") as outf:
        outf.write(f.as_srec())

infiles = ["level1", "level2", "model3"]
for infile in infiles:
    convBinToSREC(infile + ".rom", infile + ".srec")

print("Done")
