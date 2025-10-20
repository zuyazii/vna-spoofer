import sys
from PySide6.QtWidgets import QApplication
from views.sidebar import SidebarView

app = QApplication(sys.argv)
side = SidebarView()
side.show()
app.exec()
