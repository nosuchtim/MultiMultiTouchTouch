import java.util.*;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileInputStream;
import java.io.StringReader;
import java.io.FileNotFoundException;
import java.io.IOException;

import processing.net.*;
import oscP5.*;
import netP5.*;
import processing.opengl.*;

String filesDir = "/users/tjt/documents/processing/tjt_particles1";

ArrayList clients = new ArrayList();
byte[] byteBuffer = new byte[256];
byte newline = 10;
 
OscP5 oscP5;

import themidibus.*;
MidiBus myMidi;

// ControlP5 controlP5;
Bird bird;
ArrayList birds;
ArrayList cursors;
float maxvel=20;
float maxmaxvel=10;
int startbirds=10;   // was 20000
PVector mouse;
float cap;
boolean draw_background=false;
boolean drawtrails=true;
int numbirdareas = 7;
int numareas = 8;
ArrayList areas;
// int numalive = 0;
float zmult = 200.0;
int minsize = 4;
int maxsize = 30;
float vel_mult_active = 4.0;
float vel_mult_normal = 4.0;
float alpha = 0.2;
// float fade = 0.0058;
float fade = 0.1;

void setup() {

  size(800,600);
  background(0);

  myMidi = new MidiBus(this, -1, "loopMIDI Port 1");

  oscP5 = new OscP5(this,3333);

  mouse = new PVector(mouseX,mouseY);
  birds = new ArrayList();

  // controlP5 = new ControlP5(this);
  // controlP5.addButton("background",0,20,20,80,19);
  // controlP5.addButton("trails",0,100,20,80,19);

  areas = new ArrayList();
  for(int a=0;a<numareas;a++) {
    MultiTouchArea area = new MultiTouchArea();
    areas.add(area);

	switch(a%numbirdareas){
	case 0: area.setcolor(255,0,0); break;
	case 1: area.setcolor(0,255,0); break;
	case 2: area.setcolor(0,0,255); break;
	case 3: area.setcolor(255,255,0); break;
	case 4: area.setcolor(0,255,255); break;
	case 5: area.setcolor(255,0,255); break;
	case 6: area.setcolor(255,255,255); break;
	default: break;
	}

      for(int i=0;i<startbirds;i++) {
        birds.add(new Bird(int(random(0,width)),int(random(0,height)),random(-1,1),random(-1,1),birds,birds.size()+1,area));
      }
    }
    set_alpha_all(alpha);

}

void set_alpha_all(float a) {
      for ( int i=0; i<numareas; i++ ) {
        MultiTouchArea area = (MultiTouchArea) areas.get(i);
        area.set_alpha(a);
      }        
}

/* incoming osc message are forwarded to the oscEvent method. */
void oscEvent(OscMessage m) {
  /* print the address pattern and the typetag of the received OscMessage */
  // print("### TJT PARTICLES received an osc message.");
  // print(" addrpattern: "+m.addrPattern());
  // println(" typetag: "+m.typetag());
  Object[] args = m.arguments();
  // println("OSCEVENT addr="+m.addrPattern());

  if (m.checkAddrPattern("/tuio/25Dblb")) {

    String cmd = m.get(0).stringValue();
    // println("NEW CMD=("+cmd+")");
    if ( cmd.equals("alive") ) {
      // println("ALIVE! typetag="+m.typetag());
      int na = m.typetag().length() - 1;
      for ( int a=0; a<numareas; a++ ) {
        MultiTouchArea area = (MultiTouchArea) areas.get(a);
        area.cleartouched();
        area.setalive(0);
      }        
      for ( int i=1; i<=na; i++ ) {
        int sid = m.get(i).intValue();
        int areanum = (sid/1000) - 1;
        if ( areanum >= numareas ) {
          println("HEY! areanum from osc = "+areanum+" !?");
          continue;
        }
        MultiTouchArea area = (MultiTouchArea) areas.get(areanum);
        if ( areanum < numbirdareas ) {
          area.incalive();
        } else {
          // Do button/slider areas
          area.settouched();
        }
      }

//      for ( int a=0; a<numareas; a++ ) {
//        if ( a >= numbirdareas ) {
//          MultiTouchArea area = (MultiTouchArea) areas.get(a);
//          if ( area.touched == 1 ) {
//            if ( area.onoff == 0 ) {
//              println("TURNING AREA "+a+" ON");
//              area.onoff = 1;
//              if ( a == 5 ) {
//                println("TOGGLING DRAWTRAILS");
//                drawtrails = !drawtrails;
//              }
//            }
//          } else {
//            // area wasn't touched, so turn it off if it's on
//            if ( area.onoff == 1 ) {
//              println("TURNING AREA "+a+" OFF");
//              area.onoff = 0;
//            }
//          }
//        }
//      }        

      // println("NUMalive now "+numalive);
    } else if (cmd.equals("fseq") ) {
    } else if ( cmd.equals("set") ) {
      int sid = m.get(1).intValue();
      float x = m.get(2).floatValue();
      float y = m.get(3).floatValue();
      float z = m.get(4).floatValue();
      y = 1.0 - y;
      // println("RAW z="+z);
      float a = m.get(8).floatValue();
      int areanum = (sid/1000) - 1;
      MultiTouchArea area = (MultiTouchArea) areas.get(areanum);
      int w = (int)(z*zmult);
      /* if ( w > 10 ) {
        println("osc set, w>10 = "+w+" raw z="+z);
      } */
      area.setcursor((int)(x*width),(int)(y*height),(int)(z*zmult));
      int pitch = (int)(x * 127);
      int vel = (int)((z*3) * 127);
      if ( vel > 127 ) {
        vel = 127;
      }
      println("Note pitch="+pitch+" vel="+vel);
      myMidi.sendNoteOn(0,pitch,vel);
    }
  }
  // println("End of OSCEVENT");
}
 
void draw() {
	
	if(draw_background){
		background(0);
	} else {
		fill(0,0,0,(int)(255*fade));
		rect(0,0,width,height);
	}

  //if(drawtrails){fill(0,60);rect(0,0,width,height);}
   
  // controlP5.draw();
  for(int i=0;i<birds.size();i++) {
    Bird bird = (Bird) birds.get(i);
    bird.move();
    bird.display();
    bird.flock();
  }

  
  mouse.x =mouseX;
  mouse.y=mouseY;
   
}
 
class Cursor {
  int x;
  int y;
  int z;
}

class MultiTouchArea {
  Cursor cursor;
  int r;
  int g;
  int b;
  int a;
  int numalive;
  int touched;
  int onoff;
  MultiTouchArea() {
    cursor = new Cursor();
    numalive = 0;
    touched = 0;
    onoff = 0;
  }
  void settouched() {
    touched = 1;
  }
  void cleartouched() {
    touched = 0;
  }
  void setalive(int a) {
    numalive = a;
  }
  boolean isalive() {
    return (numalive>0);
  }
  void incalive() {
    numalive++;
  }
  void setcolor(int r_, int g_, int b_) {
    r = r_;
    g = g_;
    b = b_;
  }
  void set_alpha(float a_) {
    a = (int)Math.floor(a_ * 255);
  }
  void setcursor(int x_, int y_, int z_) {
    cursor.x = x_;
    cursor.y = y_;
    /* if ( z_ > 0 ) {
      println("setcursor non-zero z="+z_);
    } */
    cursor.z = z_;
  }
}

class Bird {
   
  PVector location;
  PVector velocity;
  PVector acceleration=new PVector(0,0);
  PVector dir=new PVector(0,0);
  int siz;
  ArrayList others;
  int id;
  float dirmag;
  Bird other;
  MultiTouchArea mtarea;
   
  Bird(int startx,int starty, float startxvel,float startyvel,ArrayList others_,int id_,MultiTouchArea mtarea_) {
    // println("Bird constructor xy="+startx+" "+starty);
    location = new PVector(startx,starty);
    velocity = new PVector(startxvel,startyvel);
    // siz=siz_;
    others = others_;
    id=id_;
    if(id>1){other = (Bird) others.get(int(random(others.size())));} else { other = this;}
    mtarea = mtarea_;
  }
  void flock() {
     if(mtarea.numalive > 0) {
       PVector v = new PVector(mtarea.cursor.x,mtarea.cursor.y);
        dir = PVector.sub(v,location);
        dirmag=dir.mag();
        dir.normalize();
        dir.mult(vel_mult_active);
        // println("numalive="+numalive);
        //cap=maxmaxvel;
      } else {
        dir = PVector.sub(other.location,location);
        dir.normalize();
        dir.mult(vel_mult_normal);
      }
 
      acceleration=dir;
     }
  void bound() {
	if ( location.x < 0 ) {
		location.x = 0;
	} else if ( location.x > width ) {
		location.x = width;
	}
	if ( location.y < 0 ) {
		location.y = 0;
	} else if ( location.y > height ) {
		location.y = height;
	}
  }
  void move() {
    velocity.add(acceleration);
    velocity.limit(maxvel);
     
    location.add(velocity);
    bound();
    velocity.mult(0.99);
  }
   
  void display() {
    // stroke(velocity.mag()*40+50,acceleration.mag()*100+100,velocity.mag()*100+100,dirmag+20);
    if ( ! mtarea.isalive() ) {
	return;
    }
    stroke(mtarea.r,mtarea.g,mtarea.b,mtarea.a);
    int w = mtarea.cursor.z;
    // println("mtarea.z="+mtarea.cursor.z);
    if ( w < minsize ) {
      w = minsize;
    } else if ( w > maxsize ) {
      w = maxsize;
    } else {
      // println("w >= 2 < 10 = "+w);
    }
    if(!drawtrails){
      strokeWeight(w);
      point(location.x,location.y);
    } else {
      strokeWeight(3);
      int zval = w / 3;
      line(location.x,location.y,location.x-velocity.x*zval,location.y-velocity.y*zval);
      // line(location.x,location.y,location.x-velocity.x*3,location.y-velocity.y*3);
    } 
  }
}
