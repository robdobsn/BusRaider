// Bus Raider
// Rob Dobson 2019

#include "TermH19.h"
#include "ascii.h"

// Log string
static const char *MODULE_PREFIX = "TermH19";

TermH19::TermH19()
{
}

TermH19::~TermH19()
{
}

void TermH19::init(uint32_t cols, uint32_t rows)
{
    TermEmu::init(cols, rows);
    reset();
}

void TermH19::reset()
{
    TermEmu::reset();
    mode_m = Normal;
    reverseVideo_m = false;
    graphicMode_m = false;
    insertMode_m = false;
    line25_m = false;
    holdScreen_m = false;
    _cursor._off = false;
    altKeypadMode_m = false;
    keyboardEnabled_m = true;
    _ansiMode = false;
    _rowsMain = _rows - 1;
}

void TermH19::putChar(uint32_t ch)
{
    ch &= 0x7f;

    if (mode_m == Normal)
    {
        switch (ch)
        {
        case ascii::NUL:
        case ascii::SOH:
        case ascii::STX:
        case ascii::ETX:
        case ascii::EOT:
        case ascii::ENQ:
        case ascii::ACK:
        case ascii::VT:
        case ascii::FF:
        case ascii::SO:
        case ascii::SI:
        case ascii::DLE:
        case ascii::DC1:
        case ascii::DC2:
        case ascii::DC3:
        case ascii::DC4:
        case ascii::NAK:
        case ascii::SYN:
        case ascii::ETB:
        case ascii::EM:
        case ascii::SUB:
        case ascii::FS:
        case ascii::GS:
        case ascii::RS:
        case ascii::US:
        case ascii::DEL:
            // From manual, these characters are not processed by the terminal
            break;

        case ascii::BEL: // Rings the bell.
            /// \todo - implement ringing bell.
            consoleLog("<BEL>");
            break;

        case ascii::BS: // Backspace
            consoleLog("<BS>");
            processBS();
            break;

        case ascii::HT: // Horizontal Tab
            consoleLog("<TAB>");
            processTAB();
            break;

        case ascii::LF: // Line Feed
            processLF();

            if (autoCR_m)
            {
                processCR();
            }

            consoleLog("\n");
            break;

        case ascii::CR: // Carriage Return
            processCR();

            if (autoLF_m)
            {
                processLF();
            }

            break;

        case ascii::CAN: // Cancel.
            break;

        case ascii::ESC: // Escape
            mode_m = Escape;
            consoleLog("<ESC>");
            break;

        default:
            // if Printable character display it.
            displayCharacter(ch);
            break;
        }
    }
    else if (mode_m == Escape)
    {
        // Assume we go back to Normal, so only the few that don't need to set the mode.
        mode_m = Normal;

        switch (ch)
        {
        case ascii::CAN: // CAN - Cancel
            // return to Normal mode, already set.
            break;

        case ascii::ESC: // Escape
            // From the ROM listing, stay in this mode.
            mode_m = Escape;
            break;

            // Cursor Functions

        case 'H': // Cursor Home
            _cursor._col = _cursor._row = 0;
            _cursor._updated = true;
            break;

        case 'C': // Cursor Forward
            cursorForward();
            break;

        case 'D':        // Cursor Backward
            processBS(); // same processing as cursor backward
            break;

        case 'B': // Cursor Down
            cursorDown();
            break;

        case 'A': // Cursor Up
            cursorUp();
            break;

        case 'I': // Reverse Index
            reverseIndex();
            break;

        case 'n': // Cursor Position Report
            cursorPositionReport();
            break;

        case 'j': // Save cursor position
            saveCursorPosition();
            break;

        case 'k': // Restore cursor position
            restoreCursorPosition();
            break;

        case 'Y': // Direct Cursor Addressing
            mode_m = DCA_1;
            break;

            // Erase and Editing

        case 'E': // Clear Display
            clearDisplay();
            break;

        case 'b': // Erase Beginning of Display
            eraseBOD();
            break;

        case 'J': // Erase to End of Page
            eraseEOP();
            break;

        case 'l': // Erase entire Line
            eraseEL();
            break;

        case 'o': // Erase Beginning Of Line
            eraseBOL();
            break;

        case 'K': // Erase To End Of Line
            eraseEOL();
            break;

        case 'L': // Insert Line
            insertLine();
            break;

        case 'M': // Delete Line
            deleteLine();
            break;

        case 'N': // Delete Character
            deleteChar();
            break;

        case '@': // Enter Insert Character Mode
            insertMode_m = true;
            break;

        case 'O': // Exit Insert Character Mode
            insertMode_m = false;
            break;

            // Configuration

        case 'z': // Reset To Power-Up Configuration
            reset();
            break;

        case 'r': // Modify the Baud Rate
            break;

        case 'x': // Set Mode
            mode_m = SetMode;
            break;

        case 'y': // Reset Mode
            mode_m = ResetMode;
            break;

        case '<': // Enter ANSI Mode
            // implement ANSI mode.
            setAnsiMode(true);
            break;

            // Modes of operation

        case '[': // Enter Hold Screen Mode
            holdScreen_m = true;
            break;

        case '\\': // Exit Hold Screen Mode
            holdScreen_m = false;
            break;

        case 'p': // Enter Reverse Video Mode
            reverseVideo_m = true;
            break;

        case 'q': // Exit Reverse Video Mode
            reverseVideo_m = false;
            break;

        case 'F': // Enter Graphics Mode
            graphicMode_m = true;
            break;

        case 'G': // Exit Graphics Mode
            graphicMode_m = false;
            break;

        case 't': // Enter Keypad Shifted Mode
            keypadShifted_m = true;
            break;

        case 'u': // Exit Keypad Shifted Mode
            // ROM - just sets the mode
            keypadShifted_m = false;
            break;

        case '=': // Enter Alternate Keypad Mode
            // ROM - just sets the mode
            keypadShifted_m = true;
            break;

        case '>': // Exit Alternate Keypad Mode
            // ROM - just sets the mode
            keypadShifted_m = false;
            break;

            // Additional Functions

        case '}': // Keyboard Disable
            /// \todo - determine whether to do this.
            keyboardEnabled_m = false;
            break;

        case '{': // Keyboard Enable
            keyboardEnabled_m = true;
            break;

        case 'v': // Wrap Around at End Of Line
            wrapEOL_m = true;
            break;

        case 'w': // Discard At End Of Line
            wrapEOL_m = false;
            break;

        case 'Z': // Identify as VT52 (Data: ESC / K)
            sendData(ascii::ESC);
            sendData('/');
            sendData('K');
            break;

        case ']': // Transmit 25th Line
            transmitLine25();
            break;

        case '#': // Transmit Page
            transmitPage();
            break;

        default:
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "UnhandledESC %02x", ch);
            break;
        }
    }
    else if (mode_m == SetMode)
    {
        mode_m = Normal;

        switch (ch)
        {
        case '1': // Enable 25th line
            // From the ROM, it erases line 25 on the enable, but here we erase on the disable.
            line25_m = true;
            break;

        case '2': // No key click
            keyClick_m = true;
            break;

        case '3': // Hold screen mode
            holdScreen_m = true;
            break;

        case '4': // Block Cursor
            _cursor._blockCursor = true;
            _cursor._updated = true;
            break;

        case '5': // Cursor Off
            _cursor._off = true;
            _cursor._updated = true;
            break;

        case '6': // Keypad Shifted
            keypadShifted_m = true;
            break;

        case '7': // Alternate Keypad mode
            altKeypadMode_m = true;
            break;

        case '8': // Auto LF
            autoLF_m = true;
            break;

        case '9': // Auto CR
            autoCR_m = true;
            break;

        default:
            /// Need to process ch as if none of this happened...
            break;
        }
    }
    else if (mode_m == ResetMode)
    {
        mode_m = Normal;

        switch (ch)
        {
        case '1': // Disable 25th line
            eraseLine(_rowsMain);
            line25_m = false;
            _cursor._updated = true;
            break;

        case '2': // key click
            keyClick_m = false;
            break;

        case '3': // Hold screen mode
            holdScreen_m = false;
            break;

        case '4': // Block Cursor
            _cursor._blockCursor = false;
            _cursor._updated = true;
            break;

        case '5': // Cursor On
            _cursor._off = false;
            _cursor._updated = true;
            break;

        case '6': // Keypad Unshifted
            keypadShifted_m = false;
            break;

        case '7': // Exit Alternate Keypad mode
            altKeypadMode_m = false;
            break;

        case '8': // No Auto LF
            autoLF_m = false;
            break;

        case '9': // No Auto CR
            autoCR_m = false;
            break;

        default:
            break;
        }
    }
    else if (mode_m == DCA_1)
    {
        // From actual H19, once the line is specified, the cursor
        // immediately moves to that line, so no need to save the
        // position and wait for the entire command.
        /// \todo verify that this is the same on newer H19s.
        if (ch == ascii::CAN)
        {
            // cancel
            mode_m = Normal;
        }
        else
        {
            // \todo handle error conditions

            int pos = ch - 31;

            // verify valid Position
            if (((pos > 0) && (pos < (signed)_rows)) || ((pos == (signed)_rows) && (line25_m)))
            {
                _cursor._row = pos - 1;
            }
            else
            {
                /// \todo check to see how a real h19 handles this.
                LogWrite(MODULE_PREFIX, LOG_DEBUG, "DCA invalid Y: %d\n", pos);
            }

            mode_m = DCA_2;
        }
    }
    else if (mode_m == DCA_2)
    {
        if (ch == ascii::CAN)
        {
            // cancel
            mode_m = Normal;
        }
        else
        {
            int pos = ch - 31;

            if ((pos > 0) && (pos < 81))
            {
                _cursor._col = pos - 1;
            }
            else
            {
                _cursor._col = (_cols - 1);
            }

            _cursor._updated = true;
            mode_m = Normal;
        }
    }
}

void TermH19::setAnsiMode(bool on)
{
    _ansiMode = on;
}

/// \brief Process Carriage Return
///
void TermH19::processCR()
{
    // check to possibly save the update.
    if (_cursor._col)
    {
        _cursor._col = 0;
        _cursor._updated = true;
    }
}

/// \brief Process Line Feed
///
/// \todo - verify line 25 handling. make sure it doesn't clear line 25.
void TermH19::processLF()
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "processLF row %d col %d rowsMain %d", _cursor._row, _cursor._col, _rowsMain);
    if (onLine25())
    {
        // On line 25, don't do anything
        return;
    }

    // Determine if we can just move down a row, or if we have to scroll.
    if (_cursor._row < (_rowsMain - 1))
    {
        ++_cursor._row;
    }
    else
    {
        // must be line 24 - have to scroll.
        scroll();
    }

    _cursor._updated = true;
}

/// \brief Process Backspace
///
void TermH19::processBS()
{
    if (_cursor._col)
    {
        --_cursor._col;
        _cursor._updated = true;
    }
}

/// \brief Process TAB
///
void TermH19::processTAB()
{
    if (_cursor._col < 72)
    {
        _cursor._col += 8;
        // mask off the lower 3 bits to get the correct column.
        _cursor._col &= 0xf8;
        _cursor._updated = true;
    }
    else if (_cursor._col < (_cols - 1))
    {
        _cursor._col++;
        _cursor._updated = true;
    }
}

/// \brief Process Cursor Home
/// \todo - Determine how these function in relation to line 25..
void TermH19::cursorHome()
{
    if (_cursor._col || _cursor._row)
    {
        _cursor._col = 0;
        _cursor._row = 0;
        _cursor._updated = true;
    }
}

/// \brief Process Cursor Forward
///
void TermH19::cursorForward()
{
    // ROM comment says that it will wrap around when at pos 80, but the code does not do that,
    // and verifying on a real H89, even with wrap-around enabled, the cursor will not wrap.
    if (_cursor._col < (_cols - 1))
    {
        ++_cursor._col;
        _cursor._updated = true;
    }
}

/// \brief Process cursor down
///
void TermH19::cursorDown()
{
    // ROM - Moves the cursor down one line on the display but does not cause a scroll past
    // the last line
    if (_cursor._row < (_rowsMain - 1))
    {
        ++_cursor._row;
        _cursor._updated = true;
    }
}

/// \brief Process cursor up
/// \todo determine if this is correct with line 25.
void TermH19::cursorUp()
{
    if (_cursor._row)
    {
        --_cursor._row;
        _cursor._updated = true;
    }
}

///
/// \brief Process reverse index
///
void TermH19::reverseIndex()
{
    // Check for being on line 25
    if (onLine25())
    {
        /// \todo - is this right? or should we clear the 25th line?
        return;
    }

    // Make sure not first line
    if (_cursor._row)
    {
        // simply move up a row
        --_cursor._row;
    }
    else
    {
        // must be line 0 - have to scroll down.
        for (int y = (_rowsMain - 1); y > 0; --y)
        {
            for (unsigned int x = 0; x < _cols; ++x)
            {
                getTermChar(y * _cols + x) = getTermChar((y - 1) * _cols + x);
            }
        }

        eraseLine(0);
    }

    _cursor._updated = true;
}

/// \brief Process cursor position report
///
/// Send ESC Y <_cursor._row+0x20> <_cursor._col+0x20>
void TermH19::cursorPositionReport()
{
    // Send ESC Y <_cursor._row+0x20> <_cursor._col+0x20>
    sendData(ascii::ESC);
    sendData('Y');
    sendData(_cursor._row + 0x20);
    sendData(_cursor._col + 0x20);
}

/// \brief Process save cursor position
///
void TermH19::saveCursorPosition()
{
    saveX_m = _cursor._col;
    saveY_m = _cursor._row;
}

/// \brief process restore cursor position
///
void TermH19::restoreCursorPosition()
{
    _cursor._col = saveX_m;
    _cursor._row = saveY_m;
    _cursor._updated = true;
}

// Erasing and Editing

/// \brief Clear Display
void TermH19::clearDisplay()
{
    // if on line 25, then only erase line 25
    if (onLine25())
    {
        eraseEL();
        /// \todo determine if '_cursor._col = 0' is needed.
    }
    else
    {
        for (unsigned int y = 0; y < _rowsMain; ++y)
        {
            eraseLine(y);
        }

        _cursor._col = 0;
        _cursor._row = 0;
    }

    _cursor._updated = true;
}

/// \brief Erase to Beginning of display
///
void TermH19::eraseBOD()
{
    eraseBOL();

    // if on line 25 just erase to beginning of line
    if (!onLine25())
    {
        int y = _cursor._row - 1;

        while (y >= 0)
        {
            eraseLine(y);
            --y;
        }
    }

    _cursor._updated = true;
}

/// \brief Erase to End of Page
///
void TermH19::eraseEOP()
{
    eraseEOL();
    unsigned int y = _cursor._row + 1;

    /// \todo what about line 25?
    while (y < _rowsMain)
    {
        eraseLine(y);
        ++y;
    }
    _cursor._updated = true;
}

/// \brief Erase to End of Line
///
void TermH19::eraseEL()
{
    eraseLine(_cursor._row);
    _cursor._updated = true;
}

/// \brief Erase to beginning of line
///
void TermH19::eraseBOL()
{
    int x = _cursor._col;

    do
    {
        getTermChar(_cursor._row * _cols + x)._charCode = ascii::SP;
        x--;
    } while (x >= 0);

    _cursor._updated = true;
}

/// \brief erase to end of line
///
void TermH19::eraseEOL()
{
    unsigned int x = _cursor._col;

    do
    {
        getTermChar(_cursor._row * _cols + x)._charCode = ascii::SP;
        x++;
    } while (x < _cols);

    _cursor._updated = true;
}

/// \brief insert line
///
void TermH19::insertLine()
{
    /// \todo - Determine how the REAL H89 does this on Line 25, the ROM listing is not clear.
    /// - a real H19 messes up with either an insert or delete line on line 25.
    /// - note tested with early H19, newer H19 roms should have this fixed.
    for (unsigned int y = (_rowsMain - 1); y > _cursor._row; --y)
    {
        for (unsigned int x = 0; x < _cols; ++x)
        {
            getTermChar(y * _cols + x) = getTermChar((y - 1) * _cols + x);
        }
    }

    eraseLine(_cursor._row);

    _cursor._col = 0;

    _cursor._updated = true;
}

/// \brief delete line
///
/// \todo - Determine how the REAL H89 does this on Line 25, the ROM listing is not clear.
/// - a real H19 messes up with either an insert or delete line on line 25.
/// - note tested with early H19, newer H19 roms should have this fixed.
void TermH19::deleteLine()
{
    // Go to the beginning of line
    _cursor._col = 0;

    // move all the lines up.
    for (unsigned int y = _cursor._row; y < (_rowsMain - 1); ++y)
    {
        for (unsigned int x = 0; x < _cols; ++x)
        {
            getTermChar(y * _cols + x) = getTermChar((y + 1) * _cols + x);
        }
    }

    // clear line 24.
    eraseLine((_rowsMain - 1));
    _cursor._updated = true;
}

/// \brief delete character
///
void TermH19::deleteChar()
{
    // move all character in.
    for (unsigned int x = _cursor._col; x < (_cols - 1); x++)
    {
        getTermChar(_cursor._row * _cols + x) = getTermChar(_cursor._row * _cols + x + 1);
    }

    // clear the last column
    getTermChar(_cursor._row * _cols + _cols - 1)._charCode = ascii::SP;
    _cursor._updated = true;
}

void TermH19::eraseLine(unsigned int line)
{
    if (line >= _rows)
        return;

    for (unsigned int x = 0; x < _cols; ++x)
    {
        getTermChar(line * _cols + x)._charCode = ascii::SP;
    }
};

/// \brief Process enable line 25.
///
void TermH19::processEnableLine25()
{
    // From the ROM, it erases line 25 on the enable, but here we erase on the disable.
    //
}

void TermH19::displayCharacter(unsigned int ch)
{
    // when in graphic mode, use this lookup table to determine character to display,
    // note: although entries 0 to 31 are defined, they are not used, since the control
    // characters are specifically checked in the switch statement.
    static char graphicLookup[0x80] =
        {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
            32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
            48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
            64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
            80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 127, 31,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 32};

    unsigned int symbol = 0;

    if (graphicMode_m)
    {
        // Look up symbol
        if (ch < sizeof(graphicLookup))
            symbol = graphicLookup[ch];
    }
    else
    {
        symbol = ch;
    }

    if (reverseVideo_m)
    {
        // set the high-bit for reverse video.
        // TODO
        // symbol |= 0x80;
    }

    if (!((_cursor._col <= _cols) && (_cursor._row < _rows)))
    {
        LogWrite(MODULE_PREFIX, LOG_ERROR, "Invalid X, Y: %d, %d\n", _cursor._col, _cursor._row);
        _cursor._col = 0;
        _cursor._row = 0;
    }

    if (insertMode_m)
    {
        for (unsigned int x = (_cols - 1); x > _cursor._col; --x)
        {
            getTermChar(_cursor._row * _cols + x) = getTermChar(_cursor._row * _cols + x - 1);
        }
    }

    if (_cursor._col >= _cols)
    {
        if (wrapEOL_m)
        {
            _cursor._col = 0;

            if (_cursor._row < (_rowsMain - 1))
            {
                _cursor._row++;
            }
            else if (_cursor._row == (_rowsMain - 1))
            {
                scroll();
            }
            else
            {
                // On a real H19, it just wraps back to column 0, and stays
                // on line 25 (24)
                //assert(_cursor._row == _rowsMain);
            }
        }
        else
        {
            _cursor._col = (_cols - 1);
        }
    }

    getTermChar(_cursor._row * _cols + _cursor._col)._charCode = symbol;
    _cursor._col++;

    _cursor._updated = true;
}
