
from opencore import *

def GameEvent(cd):
	try:
		if cd.event == EVENT_START:
			RegisterPlugin(CORE_VERSION, 'testpy', 'cycad', '1.0', '', '', 'A test Python plugin', 0)
			cd.user_data = ('asdf',)
		elif cd.event == EVENT_LOGIN:
			PubMessage("EVENT_LOGIN: %s" % cd.user_data[0])
		elif cd.event == EVENT_ENTER:
			pass
	except Exception as e:
		print 'Python Exception: %s' % str(e)

