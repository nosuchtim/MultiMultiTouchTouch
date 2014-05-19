import java.util.*;

public class Areas {

	Map<String,Area> areaMap;

	public Areas() {
		areaMap = new HashMap<String,Area>();
	}

	public void add(String name, Area a) {
		areaMap.put(name,a);
	}

	public void clearTouched() {
		for (String a: areaMap.keySet()) {
			Area area = areaMap.get(a); 
			area.clearTouched();
		}
	}

	public void checkCursorUp() {
		for (String a: areaMap.keySet()) {
			Area area = areaMap.get(a); 
			area.checkCursorUp();
		}
	}

	public Area findArea(int sid) {
		for (String a: areaMap.keySet()) {
			Area area = areaMap.get(a); 
			if ( sid >= area.sidmin && sid <= area.sidmax ) {
				return area;
			}
		}
		return null;
	}
};
