class TRS80CmdFileFormat {

    process(filename, memAccess, z80Proc) {

        const fs = require('fs');
        const pData = fs.readFileSync(filename);
        const dataLen = pData.length;
        let pos = 0;
        while (pos < dataLen - 2)
        {
            // Record start info
            let code = pData[pos++];
            let length = pData[pos++];
            length = (length < dataLen-1) ? length : (dataLen-1);
            if (length == 0)
                length = 0x100;

            // Code
            switch(code)
            {
                case 1:
                {
                    // Data segment
                    let addr = pData[pos] + ((pData[pos+1]) << 8);
                    pos+=2;
                    if (length < 3)
                        length += 0xfe;
                    else
                        length -= 2;
                    memAccess.blockWrite(addr, pData.slice(pos, pos+length), false);
                    console.log(`Code segment addr ${addr.toString(16)} len ${length} pos ${pos} pData ${pData[pos].toString(16)} ${pData[pos+1].toString(16)}`);
                    break;
                }
                case 2:
                {
                    // Exec addr
                    if (length == 1)
                    {
                        const z80State = z80Proc.getState();
                        z80State.pc = pData[pos];
                        z80Proc.setState(z80State);
                        console.log(`TRS80CmdFileFormat exec addr ${z80Proc.getState().pc.toString(16)}`);
                        pos += 1;
                    }
                    else if (length == 2)
                    {
                        const z80State = z80Proc.getState();
                        z80State.pc = pData[pos] + ((pData[pos+1]) << 8);
                        z80Proc.setState(z80State);
                        console.log(`TRS80CmdFileFormat exec addr ${z80Proc.getState().pc.toString(16)}`);
                        pos+=2;
                    }
                    else
                    {
                        console.log(`TRS80CmdFileFormat error in exec addr`);
                    }
                    break;
                }
                case 3:
                {
                    // Non exec marker
                    console.log(`TRS80CmdFileFormat non exec marker`);
                    return;
                }
                case 4:
                {
                    console.log(`TRS80CmdFileFormat end of partitioned data`);
                    break;
                }
                case 5:
                {
                    console.log(`TRS80CmdFileFormat ttile`);
                    break;
                }
                default:
                {
                    console.log(`TRS80CmdFileFormat undecoded block ${code.toString(16)}`);
                    break;
                }
            }

            // Bump pos
            pos += length;
        }
    }
}

module.exports = TRS80CmdFileFormat;