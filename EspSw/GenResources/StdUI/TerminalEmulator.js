// Terminal Emulator 
// by Rob Dobson 2018 

class TerminalEmulator
{
	constructor(id, showCursor, blinkCursor, numCols, numRows, echoInput, bufferLines)
	{
		this.html = document.createElement('div');
		this.html.className = 'TerminalEmulator';
		if (typeof(id) === 'string')
		{
			this.html.id = id;
		}

		this._terminalWindow = document.createElement('textarea');
		this._cursor = document.createElement('span');

		this._shouldBlinkCursor = blinkCursor;
		this._shouldShowCursor = showCursor;
		this._echoInput = echoInput;
		this._bufferLines = bufferLines;

		this.pruneBuffer = function()
		{
			// // Check how many lines in buffer
			// let bufLines = this._terminalWindow.value.split(/\r\n|\r|\n/);
			// if (bufLines.length > this._bufferLines)
			// {
			// 	bufLines = bufLines.splice(0, bufLines.length - this._bufferLines)
			// }
			// this._terminalWindow.value = bufLines.join("\r\n");
		}

		this.echoChar = function(strval)
		{
			this._terminalWindow.value += strval;
		}

		this.showString = function(strval)
		{
			// Make changes in a copy of the terminal window
			let curBuffer = this._terminalWindow.value;
			for (let i = 0; i < strval.length; i++)
			{
				let chVal = strval.charCodeAt(i);
				if(chVal == 0x0c)
				{
					// Clear
					curBuffer = "";
				}
				else if (chVal == 0x08)
				{
					curBuffer = curBuffer.substr(0,curBuffer.length-1);
				}
				else if (chVal == 0x0d)
				{
					// Ignore
				}
				else if (chVal == 0x0a)
				{
					curBuffer += "\n";
				}
				else
				{
					curBuffer += strval[i];
				}
			}

			// Check if doesn't exceed line limit
			let bufLines = curBuffer.split(/\n/);
			if (bufLines.length > this._bufferLines)
				bufLines.splice(0, bufLines.length - this._bufferLines);
			curBuffer = bufLines.join("\n");
			this._terminalWindow.value = curBuffer;
		}

		this.backspace = function()
		{
			this._terminalWindow.value = this._terminalWindow.value.substr(0,this._terminalWindow.value.length-1);
		}

		this.crlf = function()
		{
			this._terminalWindow.value += "\r\n";
		}

		this.tab = function()
		{
			this._terminalWindow.value += "\t";
		}

		this.setInputCallback = function (callback)
		{
			this.enableInput(callback)
		}

		this.clear = function () 
		{
			this._terminalWindow.innerHTML = ''
		}

		this.setTextSize = function (size) 
		{
			this._terminalWindow.style.fontSize = size
		}

		this.setTextColor = function (col) 
		{
			this.html.style.color = col
			this._terminalWindow.style.background = col
		}

		this.setBackgroundColor = function (col) 
		{
			this.html.style.background = col
		}

		this.setWidth = function (width) 
		{
			this.html.style.width = width
		}

		this.setHeight = function (height) 
		{
			this.html.style.height = height
		}

		this.blinkingCursor = function (bool) 
		{
			bool = bool.toString().toUpperCase()
			this._shouldBlinkCursor = (bool === 'TRUE' || bool === '1' || bool === 'YES')
		}

		this._terminalWindow.appendChild(this._cursor);
		this.html.appendChild(this._terminalWindow);

		this.setBackgroundColor('black');
		this.setTextColor('white');
		this.setTextSize('1em');
		// this.setWidth('100%');
		// this.setHeight('100%');

		this.html.style.fontFamily = 'Monaco, Courier';
		this.html.style.margin = '0';

		this._terminalWindow.cols = numCols
		this._terminalWindow.rows = numRows
		this._terminalWindow.style.padding = '2px';
		this._terminalWindow.style.margin = '0';
		this._terminalWindow.style.color = "white";
		this._terminalWindow.style.background = 'black';
		this._terminalWindow.style.height = "100%";
		this._terminalWindow.style.width = "100%";
		this._terminalWindow.style.outline = "none";
		this._terminalWindow.focus();
		this._terminalWindow.readOnly = true;

		this._cursor.style.background = 'white';
		this._cursor.innerHTML = '.';
		this._cursor.style.display = "none"

		this.clear();
		this.enableInput();
	}

	cursorBlink()
	{
		let term = this;
		setTimeout(function () 
		{
			if (term._shouldBlinkCursor)
			{
				term._cursor.style.visibility = 
						term._cursor.style.visibility === 'visible' ? 'hidden' : 'visible'
				term.cursorBlink()
			} 
			else 
			{
				term._cursor.style.visibility = 'visible'
			}
		}, 500);
	}

	enableInput(callback)
	{
		// Enable input into terminal
		if (this._shouldShowCursor)
			this._cursor.style.display = "inline"
		if (this._shouldBlinkCursor)
			this.cursorBlink();

		// Add callback
		this._textInputCallback = callback;
		let term = this;
		this._terminalWindow.onkeydown = function(e)
		{
			// Stop it being handled otherwise
			e.preventDefault();

			// Handle echo
			if (term._echoInput)
			{

				let chCode = e.key;
				if (e.key === "Backspace")
				{
					term.backspace();
				}
				else if (e.key === "Enter")
				{
					term.crlf();
				}
				else if (e.key === "Tab")
				{
					term.tab();
				}
				else if ((e.key === "Shift") || (e.key === "Alt") ||
						 (e.key === "Control") || (e.key === "CapsLock"))
				{
					// Ignore these
				}
				else
				{
					term.echoChar(e.key);
				}
			}

			// Callback
			let keyStr = e.key;
			if (keyStr == "\"")
				keyStr = "doublequotes"
			if (keyStr == "'")
				keyStr = "singlequotes"
			if (e.shiftKey && (e.key.length != 1) && (e.key != "Shift"))
				keyStr = "SHIFT-" + keyStr;
			if (e.ctrlKey && e.key != "Control")
				keyStr = "CTRL-" + keyStr;
			if (e.altKey && e.key != "Alt")
				keyStr = "ALT-" + keyStr;
			term._textInputCallback(keyStr);
		}
	}
}
