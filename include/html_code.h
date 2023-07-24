#pragma once
#include <pgmspace.h>

char page[] PROGMEM = R"(
	<!DOCTYPE html><html><head>
	<title>Local network</title>
	<style>
	body {margin-top:50px; font-family:Arial}
	body {font-size:20px; text-align:center}
	.btn {display:block; width:280px; margin:auto; padding:30px}
	.btn {font-size:30px; color:black; text-decoration:none}
	.on {background-color:SkyBlue}
	.off {background-color:LightSteelBlue}
	.zero {background-color:Thistle}
	td {font-size:30px; margin-top:50px; margin-bottom:5px}
	p {font-size:30px; margin-top:50px; margin-bottom:5px}
	</style></head>
	<body>
	<h1>ESP8266 local area network</h1>
	<table style='width:100%'><tr>
	<td>Blue LED is <span id='LEDG'>OFF</span> now</td>
	</tr></table>
	<table style='width:100%'><tr>
	<td><button class = 'btn off' id='Green LED'
		onclick = 'sendData(id)'>Press to turn Green LED ON
		</button></td>
	</tr></table>
	<p>Counter is <span id='counter'>0</span> now</p>
	<button class = 'btn zero' id = 'zero'
	onclick = 'sendData(id)'>Press to zero counter</button>
	<script>
	function sendData(butn)
	{
		var URL, variab, test;
		if (butn == 'Blue Led') {
			URL = 'LEDBurl';
			variab = 'LEDB'
		}
		else if (butn == 'zero') {
			URL = 'zeroUrl';
			variab = 'counter';
		}

		if (butn == 'Blue Led') {
			var state = document.getElementById(butn).className;
			state = (state == 'btn on' ? 'btn off' : 'btn on');
			text = (state == 'btn on' ? butn + ' OFF' : butn + ' ON');
			document.getElementById(butn).className = state;
			document.getElementById(butn).innerHTML = 'Press to turn ' + text'
		}

		var xhr = new XMLHttpRequest();
		xhr.onreadystatechange = function(butn) {
			if (this.readyState == 4 && this.status == 200)
			document.getElementById(variab).innetHTML = this.
			responseTest;
		};
		xhr.open('GET', URL, true);
		xhr.send();
	}

	setInterval(reload, 1000);
	function reload() {
		var xhr = new XMLHttpRequest();
		xhr.onreadystatechange = function(){
			if (this.readyState == 4 && this.status == 200)
			document.getElementById('counter').innerHTML = this.
			responseText;
		};
		xhr.open('GET', 'countUrl', true);
		xhr.send();
	}
	</script>
	</body></html>
)";