#pragma once

const char* HTML_style = R"(
<style type="text/css">
body {
    background-color: #ffffff;
    font-family: consolas;
    font-size: 16 px;
    color: #2c363c;
    border:  0px solid;
    display: inline-block;
    padding: 0;
    margin: 0;
    position: relative;
    vertical-align: top;
}
.ah {
    background-color: #8ee0b6;
}
.ab {
    background-color: #c8f0da;
}
.rh {
    background-color: #ff957e;
}
.rb {
    background-color: #ffcbbd;
}
.nb {
    background-color: #eaeef0;
}
.ln {
    color: #999999;
    text-align: right;
    padding-right: 4px;
}
.cd{
    overflow-x:auto;
    max-width: 100%;
}
table {
    table-layout: fixed;
    width: 100%;
}
td {
    overflow-y: auto;
    overflow-x: hidden;
    border: 0px solid;
    vertical-align: top;
    white-space: nowrap;
}
tr {
    border: 0px solid;
    vertical-align: top;
}
</style>
)";

const char* HTML_templatesplit = R"(
<!DOCTYPE html>
<html>
<head>
__STYLE__
</head>
<body>
<table>
<td width="30px">
<pre class=ln>__LINE_L__</pre>
</td>
<td width=50%>
<pre class=cd>__CODE_L__</pre>
</td>
<td width="30px">
<pre class=ln>__LINE_R__</pre>
</td>
<td width=50%>
<pre class=cd>__CODE_R__</pre>
</td>
</table>
</body>
</html>
)";

const char* HTML_templateunified = R"(
<!DOCTYPE html>
<html>
<head>
__STYLE__
</head>
<body>
<table>
<td width="30px">
<pre class=ln>__LINE_L__</pre>
</td>
<td width="30px">
<pre class=ln>__LINE_R__</pre>
</td>
<td width=100%>
<pre class=cd>__CODE__</pre>
</td>
</table>
</body>
</html>
)";