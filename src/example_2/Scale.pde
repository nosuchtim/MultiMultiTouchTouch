import java.util.*;

class Scale {

	String name;
	boolean[] has_note;
	int nnotes;

	public Scale(String nm, int... args) {
		nnotes = 128;
		has_note = new boolean[nnotes];
		name = nm;
		clear();
		int cnt = 0;
		for ( int i: args ) {
			if ( cnt++ >= 13 ) {
				println("Scale() TOO MANY ITEMS in intialization of Scale=",name);
				break;
			}
			i = i % 12;
			while ( i < nnotes ) {
				has_note[i] = true;
				i += 12;
			}
		}
	}

	int closestTo(int pitch) {
		int closestpitch = -1;
		int closestdelta= 9999;
		for ( int i=0; i<nnotes; i++ ) {
			if ( ! has_note[i] ) {
				continue;
			}
			int delta = pitch - i;
			if ( delta < 0 ) {
				delta = -delta;
			}
			if ( delta < closestdelta ) {
				closestdelta = delta;
				closestpitch = i;
			}
		}
		return closestpitch;
	}

	void clear() {
		for ( int i=0; i<nnotes; i++ ) {
			has_note[i] = false;
		}
	}

};

