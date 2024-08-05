// Example by Tom Igoe
// Modified for https://www.dfrobot.com by Lumi, Jan. 2014

/*
   This code should show one colored blob for each detected IR source (max four) at the relative position to the camera.
 */

import processing.serial.*;

int lf = 10;    
Serial myPort;  // The serial port

// int screenX = 1920;
// int screenY = 1080;

int screenX = 1280;
int screenY = 1024;

int dataConstrainX = 768;
int dataConstrainY = 1024;

int[] input = null;

int p1x = 1023;
int p1y = 1023;
int p2x = 1023;
int p2y = 1023;
int p3x = 1023;
int p3y = 1023;
int p4x = 1023;
int p4y = 1023;

int onScreenX = 0;
int onScreenY = 0;

int finalX = 0;
int finalY = 0;

int count = 0;

String inputString;

// declare variables to hold color for the four points
color p1color = color( 255, 0, 0 ); // RED
color p2color = color( 0, 255, 0 ); // GREEN
color p3color = color( 0, 0, 255 ); // BLUE
color p4color = color( 0, 0, 255 ); // BLUE
color finalColor = color( 200, 200, 200 );
color borderColor = color( 200, 200, 200 );

int borderX = 400;
int borderY = 200;

void setup() {
  // List all the available serial ports
  println(Serial.list());
  // Open the port you are using at the rate you want:
  myPort = new Serial(this, Serial.list()[2], 115200);
  myPort.clear();
  // Throw out the first reading, in case we started reading
  // in the middle of a string from the sender.
  myPort.readStringUntil(lf);

  size(1920, 1080);
  //frameRate(30);
}

void draw() {
  background(77);

  drawBorder();

  drawDataPointReversed(p1x, p1y, p1color);  
  drawDataPointReversed(p2x, p2y, p2color);
  drawVerticalLineReversed(p1x, p1y, p2x, p2y);
  //drawDataPointReversed(p3x, p3y, p3color);  
  //drawDataPointReversed(p4x, p4y, p4color); 
  drawDataPoint(finalX, finalY, finalColor);

  drawCross(onScreenX, onScreenY);

  textSize(20);
  textAlign(LEFT, TOP);
  fill(255, 255, 255);
  text("finalX: " + finalX, 10, 10);
  text("finalY: " + finalY, 10, 10 + 25);
  text("count:  " + count, 10, 10 + 25 + 25);
  text(inputString, 10, 10 + 25 + 25 + 25);
  text("p1x: " + p1x + " p1y: " + p1y  + " p2x: " + p2x  + " p2y: " + p2y, 10, 10 + 25 + 25 + 25 + 25);
}

void drawVerticalLineReversed(int x1, int y1, int x2, int y2) {
  if (x1 != 1023 && x1 != 0 && x2 != 1023 && x2 != 0) {
    drawVerticalLine(dataConstrainX - x1, dataConstrainY - y1, dataConstrainX - x2, dataConstrainY - y2);
  }
}

void drawVerticalLine(int x1, int y1, int x2, int y2) {
  int delta = 5;
  if (abs(x1 - x2) > delta) {
    stroke(color(255, 0, 0));
  } else {
    stroke(color(0, 255, 100));
  }
  strokeWeight(1);
  line(
  map(x1, 0, dataConstrainX, borderX, screenX - borderX),
  map(y1, 0, dataConstrainY, borderY, screenY - borderY),
  map(x2, 0, dataConstrainX, borderX, screenX - borderX),
  map(y2, 0, dataConstrainY, borderY, screenY - borderY));
  strokeWeight(1);
}

void drawBorder() {
  rectMode(CORNERS);
  stroke(borderColor);
  noFill();
  rect(0 + borderX, 0 + borderY, screenX - borderX, screenY - borderY);
}

void drawDataPointReversed(int px, int py, color c) {
  if (px != 1023 && px != 0) {
    drawDataPoint(dataConstrainX - px, dataConstrainY - py, c);
  }
}

void drawDataPoint(int px, int py, color c) {
  ellipseMode(RADIUS);
  fill(c);  
  ellipse(map(px, 0, dataConstrainX, borderX, screenX - borderX), map(py, 0, dataConstrainY, borderY, screenY - borderY), 20, 20);
}

void drawCross(int px, int py) {
  ellipseMode(RADIUS);
  pushMatrix();
  translate(px, py);
  noFill(); 
  stroke(0, 200, 0);
  strokeWeight(5);
  ellipse(0, 0, 40, 40);
  fill(0, 255, 0);
  stroke(0, 0, 0);
  strokeWeight(1);
  ellipse(0, 0, 3, 3);
  popMatrix();
}

void drawButton(int xval, int yval, int state, String text) { 
  ellipseMode(RADIUS);  // Set ellipseMode to RADIUS
  if (state == 0) {
    fill(255, 0, 0);
  } else {
    fill(0, 255, 0);
  }
  ellipse(xval, yval, 20, 20); //draws an ellipse with with horizontal diameter of 20px andvertical diameter of 20px. 
  textSize(12);
  textAlign(CENTER);
  fill ( 255, 255, 255 );
  text(text, xval, yval + 42);
}

void serialEvent (Serial port) {
  String inString = port.readStringUntil(lf);
  if (inString != null) {
    inString = trim(inString);
    inputString = inString;
    println(inString);
    String[] inputS = split(inString, ',');
    if (inputS.length >= 13) {
      input = int(inputS);

      p1x = input[0];
      p1y = input[1];

      p2x = input[2];
      p2y = input[3];

      p3x = input[4];
      p3y = input[5];

      p4x = input[6];
      p4y = input[7];

      finalX = input[8];
      finalY = input[9];

      onScreenX = input[10];
      onScreenY = input[11];

      count = input[12];
    }
  }
}
