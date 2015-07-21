
from opencore import *

def GameEvent(cd):
	try:
		if cd.event == EVENT_START:
			RegisterPlugin(CORE_VERSION, 'test', 'cycad', '1.0', '', '', 'A test Python plugin', 0)
			cd.user_data = None
		elif cd.event == EVENT_ENTER:
			PubMessage("Hello %s" % cd.p1.name)
		elif cd.event == EVENT_STOP:
			cd.user_data = None
	except Exception as e:
		print 'Python Exception: %s' % str(e)

