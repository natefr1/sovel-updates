import QtQuick

Canvas {
    id: root
    property bool bluetooth: false
    property bool connected: false
    property int strength: 100
    implicitWidth: 28; implicitHeight: 28
    onBluetoothChanged: requestPaint()
    onConnectedChanged: requestPaint()
    onStrengthChanged: requestPaint()
    onPaint: {
        const ctx=getContext("2d");ctx.reset();ctx.lineWidth=1.8;ctx.lineCap="round";ctx.lineJoin="round";
        ctx.strokeStyle=connected?"#F2DEFF":"#D3B4ED";
        if(bluetooth){
            ctx.beginPath();ctx.moveTo(9,9);ctx.lineTo(19,19);ctx.lineTo(14,24);ctx.lineTo(14,4);ctx.lineTo(19,9);ctx.lineTo(9,19);ctx.stroke();
        }else{
            for(let level=0;level<3;++level){
                ctx.globalAlpha=strength>=level*33?1:0.22;
                ctx.beginPath();ctx.arc(14,23,7+level*5,Math.PI*1.25,Math.PI*1.75);ctx.stroke();
            }
            ctx.globalAlpha=1;ctx.fillStyle=ctx.strokeStyle;ctx.beginPath();ctx.arc(14,23,1.6,0,Math.PI*2);ctx.fill();
        }
    }
}
