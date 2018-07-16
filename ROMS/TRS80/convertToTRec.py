import bincopy

def convBinToTREC(inname, outname):
    print("Converting", inname, "to", outname)
    f = bincopy.BinFile()
    f.add_binary_file(inname)
    with open(outname, "w") as outf:
        outf.write(f.as_srec().replace("S", "T"))

infiles = ["level1", "level2", "model3"]
for infile in infiles:
    convBinToTREC(infile + ".rom", infile + ".trec")

print("Done")
