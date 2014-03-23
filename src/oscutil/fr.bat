python oscsend.py 4444@localhost /.register 4445
python oscsend.py 4444@localhost /.list ""
python oscsend.py 4444@localhost /.list "/ffff/ff"
python oscsend.py 4444@localhost /.list "/ffff/ff/Frame_Echo"
python oscsend.py 4444@localhost /.list "/ffff/ff/Glow"
python oscsend.py 4444@localhost "/ffff/ff/Frame_Echo/default"
python oscsend.py 4444@localhost "/ffff/ff/Frame_Echo/current"
python oscsend.py 4444@localhost "/ffff/ff/bogus"
python oscsend.py 4444@localhost "/ffff/bogus"
python oscsend.py 4444@localhost "/"
python oscsend.py 4444@localhost "/ffff/ff/Frame_Echo/current" "Decay=0.1&Echo=0.0"

rem python oscsend.py 4444@localhost "/ffff/ff/Frame_Echo/current" "Decay=0.1" "Echo=0.0"
