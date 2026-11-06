import QtQuick
Canvas {
 id:root;property string symbol:"settings";property color color:"#F5EDFF";width:22;height:22;antialiasing:true
 onSymbolChanged:requestPaint();onColorChanged:requestPaint();onWidthChanged:requestPaint();onHeightChanged:requestPaint()
 onPaint:{
  const c=getContext("2d");c.reset();c.scale(width/24,height/24);c.strokeStyle=color;c.fillStyle=color;c.lineWidth=1.7;c.lineCap="round";c.lineJoin="round";
  function line(points){c.beginPath();c.moveTo(points[0],points[1]);for(let i=2;i<points.length;i+=2)c.lineTo(points[i],points[i+1]);c.stroke()}
  function box(x,y,w,h){c.strokeRect(x,y,w,h)}
  function circle(x,y,r){c.beginPath();c.arc(x,y,r,0,Math.PI*2);c.stroke()}
  if(symbol==="home"){line([3,11,12,3,21,11]);line([5,10,5,21,10,21,10,15,14,15,14,21,19,21,19,10])}
  else if(symbol==="search"){circle(10,10,6);line([15,15,21,21])}
  else if(symbol==="downloads"){line([12,3,12,15,8,11]);line([12,15,16,11]);line([4,15,4,20,20,20,20,15])}
  else if(symbol==="control"){for(let y=6;y<=18;y+=6){line([3,y,21,y]);circle(y===12?15:8,y,2)}}
  else if(symbol==="battery"){box(2,7,17,10);box(21,10,1,4);box(5,10,7,4)}
  else if(symbol==="settings"){c.beginPath();for(let i=0;i<32;i++){const a=i*Math.PI/16,r=i%4===1||i%4===2?10:8;const x=12+Math.cos(a)*r,y=12+Math.sin(a)*r;if(i)c.lineTo(x,y);else c.moveTo(x,y)}c.closePath();c.stroke();circle(12,12,3)}
  else if(symbol==="display"||symbol==="wallpaper"){box(3,4,18,13);line([8,21,16,21]);line([12,17,12,21]);if(symbol==="wallpaper")line([5,14,9,9,13,13,16,10,19,14])}
  else if(symbol==="sound"){line([4,9,8,9,13,5,13,19,8,15,4,15,4,9]);c.beginPath();c.arc(13,12,5,-.8,.8);c.stroke();c.beginPath();c.arc(13,12,8,-.8,.8);c.stroke()}
  else if(symbol==="bluetooth"){line([8,7,16,15,12,19,12,5,16,9,8,17])}
  else if(symbol==="network"){for(let r=4;r<=10;r+=3){c.beginPath();c.arc(12,19,r,-Math.PI*.78,-Math.PI*.22);c.stroke()}circle(12,19,1)}
  else if(symbol==="clock"||symbol==="screen-time"){circle(12,12,9);line([12,6,12,12,16,14])}
  else if(symbol==="user"){circle(12,8,4);c.beginPath();c.arc(12,21,7,Math.PI,Math.PI*2);c.stroke()}
  else if(symbol==="privacy"){line([12,3,20,6,19,15,12,21,5,15,4,6,12,3]);line([8,11,11,14,16,9])}
  else if(symbol==="updates"||symbol==="backup"){c.beginPath();c.arc(12,12,8,.3,Math.PI*1.8);c.stroke();line([16,3,20,5,19,9]);line([12,7,12,16,9,13]);line([12,16,15,13])}
  else if(symbol==="keyboard"){box(2,6,20,13);for(let x=5;x<21;x+=4){line([x,9,x+1,9]);line([x,12,x+1,12])}line([7,16,17,16])}
  else if(symbol==="notifications"){line([5,17,7,14,7,9,9,5,15,5,17,9,17,14,19,17,5,17]);c.beginPath();c.arc(12,18,2,0,Math.PI);c.stroke()}
  else if(symbol==="power"){c.beginPath();c.arc(12,13,8,-.9,Math.PI*1.3);c.stroke();line([12,2,12,12])}
  else if(symbol==="storage"){box(3,4,18,16);line([3,14,21,14]);circle(17,17,1)}
  else if(symbol==="dock"||symbol==="menubar"){box(2,3,20,18);if(symbol==="dock")box(6,15,12,3);else line([2,7,22,7])}
  else if(symbol==="apps"||symbol==="widgets"){box(4,4,6,6);box(14,4,6,6);box(4,14,6,6);box(14,14,6,6)}
  else if(symbol==="developer"){line([8,6,2,12,8,18]);line([16,6,22,12,16,18]);line([14,3,10,21])}
  else if(symbol==="language"){circle(12,12,9);line([3,12,21,12]);line([12,3,9,7,8,12,9,17,12,21]);line([12,3,15,7,16,12,15,17,12,21])}
  else if(symbol==="printer"){box(6,3,12,5);box(3,8,18,10);box(6,14,12,7);line([16,11,18,11])}
  else if(symbol==="jared"){line([9,5,17,5,17,15,12,20,7,20,4,17]);line([17,10,21,10]);circle(5,5,1.5);circle(21,10,1)}
  else if(symbol==="sovel"){line([12,2,15,9,22,12,15,15,12,22,9,15,2,12,9,9,12,2])}
  else if(symbol==="appearance"){circle(12,12,9);c.beginPath();c.arc(12,12,9,Math.PI/2,Math.PI*1.5);c.fill()}
  else if(symbol==="accessibility"){circle(12,4,2);line([3,9,21,9]);line([12,6,12,14,7,22]);line([12,14,17,22])}
  else if(symbol==="sharing"){circle(5,12,3);circle(19,5,3);circle(19,19,3);line([8,10,16,6]);line([8,14,16,18])}
  else if(symbol==="gaming"){line([6,6,18,6,22,18,19,21,15,16,9,16,5,21,2,18,6,6]);line([7,9,7,14]);line([5,12,10,12]);circle(17,11,1)}
  else if(symbol==="lock"){c.strokeRect(5,11,14,10);c.beginPath();c.arc(12,11,4.5,Math.PI,0);c.stroke();line([12,15,12,17])}
  else if(symbol==="vault"){c.strokeRect(3,4,18,16);circle(12,12,4.5);line([12,7.5,12,9]);line([12,15,12,16.5]);line([7.5,12,9,12]);line([15,12,16.5,12]);line([6,20,6,22]);line([18,20,18,22])}
  else if(symbol==="exit"){line([10,4,4,4,4,20,10,20]);line([9,12,21,12]);line([17,8,21,12,17,16])}
  else {circle(12,12,8);circle(12,12,3)}
 }
}
