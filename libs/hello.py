
from opencore import *

def GameEvent(cd):
	try:
		if cd.event == EVENT_START:
			RegisterPlugin(CORE_VERSION, 'hello', 'cycad', '1.0', '', '', 'A Python plugin that greets players in pub chat', 0, 0)
		elif cd.event == EVENT_ENTER:
			PubMessage("Hello %s" % cd.p1.name)
		elif cd.event == EVENT_STOP:
			pass
	except Exception as e:
		print 'Python Exception: %s' % str(e)

