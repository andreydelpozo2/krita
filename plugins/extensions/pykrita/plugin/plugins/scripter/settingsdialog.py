# -*- coding: utf-8 -*-

from PyQt5.QtWidgets import *
from scripter import syntaxstylescombobox, fontscombobox


class SettingsDialog(QDialog):

    def __init__(self, scripter, parent=None):
        super(SettingsDialog, self).__init__(parent)

        self.scripter = scripter
        self.mainLayout = QFormLayout(self)
        self.mainLayout.addRow('Syntax Highlither', syntaxstylescombobox.SyntaxStylesComboBox(scripter.highlight))
        self.mainLayout.addRow('Fonts', fontscombobox.FontsComboBox(scripter.editor))