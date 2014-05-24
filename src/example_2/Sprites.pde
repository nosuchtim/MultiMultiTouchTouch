import java.util.*;

int spritenum = 0;

public class Sprites extends HashMap {

	synchronized public void add(Sprite s) {
		spritenum++;
		this.put(spritenum,s);
	}

	synchronized public void draw() {
		for (Object o: this.values()) {
			Sprite s = (Sprite)o;
			s.draw();
		}
	}

	synchronized public void advanceTime() {
		for(Iterator it = this.entrySet().iterator(); it.hasNext(); ) {
			Map.Entry entry = (Map.Entry) it.next();
			Sprite s = (Sprite)(entry.getValue());
			s.advanceTime();
			if(s.isdead()) {
				it.remove();
			}
		}
	}
};
