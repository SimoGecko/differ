# Δ Differ
A simple and performant command-line diff tool that opens the result in the browser.


### Use
Run using `differ.exe file1.txt file2.txt [options]` with the files you want to diff and eventually options.  

Available options:
  * `unifiedview`/`splitview`: how to display the result (single panel/double panel side-by-side).
  * `collapselines`/`expandelines`: show all lines in the file or only those with differences and their context.
  * `contextsize <int>`: how many lines of context to display for the collapselines option. 3 by default.

  * `ignorewhitespace`: ignore whitespace differences. Off by default.
  * `ignorecase`: ignore case differences. Off by default.
  * `lcs`/`patience`: the method to use to compare. "patience" by default.

You can configure external tools to use differ. For example, in SourceTree under `Tools > Options > Diff`, you can set  
	External Diff Tool: Custom  
	Diff Command = "path/to/Differ.exe"  
	Arguments="\"$LOCAL\" \"$REMOTE\" splitview patience collapselines"  

### Config
You can modify the look of the output by modifying template_split.html or template_unified.html


<div style="width: 10px; height: 10px; background: #ffcbbd;"></div>

### TODO
  - improve diff LCS and empty lines
  - fix html scrolling background missing
  - provide option to tradeoff between number of edits vs total lengh of edits


### bugs:
  - last line of diff doesn't show grey background (remove additional new lines) - (happens only left-side?)
  - related to above, some mismatch in line numbers in diff
  - (ignore spaces in hash computation to avoid opening brackets to match up incorrectly) - parameter
  - even if the hashes match up, store both L/R copies in the structure since spaces might be different but ignored
  - support non-ascii strings (and sanitize the input with non-printable characters I use)


| label          | light   | dark    | old     |
|----------------|---------|---------|---------|
| add background | `#c8f0da` | `#023121` | `#b5efdb` |
| add highlight  | `#8ee0b6` | `#057f56` | `#6bdfb8` |
| rem background | `#ffcbbd` | `#560400` | `#ffc4c1` |
| rem highlight  | `#ff957e` | `#af0900` | `#ff8983` |
| background     | `#ffffff` | `#161b1d` | `#ffffff` |
| neutral        | `#eaeef0` | `#2c363c` | `#dadada` |
| font color     | `#2c363c` | `#f7f8f9` | `#      ` |
| lines color    | `#999999` | `#999999` | `#      ` |