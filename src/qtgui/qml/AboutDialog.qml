import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// About: the legacy text (mainWindow_c::cb_About), with Qt in the credits.
BtDialog {
    id: root
    objectName: "shell.about.dialog"
    title: qsTr("About BurrTools")
    width: 520

    contentItem: Text {
        objectName: "shell.about.text"
        wrapMode: Text.WordWrap
        color: Theme.text
        font.pixelSize: Theme.fontBody
        lineHeight: 1.35
        text: "This is the GUI for BurrTools " + App.version + "\n" +
              "BurrTools (c) 2003-2025 by Andreas Röver\n" +
              "with patches from Arne Köhn, Bryan Turner, Derek Bosch, Michael Brown\n" +
              "The latest version is available at github.com/burr-tools/burr-tools\n\n" +
              "This software is distributed under the GPL\n" +
              "You should have received a copy of the GNU General Public License " +
              "along with this program (COPYING); if not, write to the Free Software " +
              "Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA " +
              "or see www.fsf.org\n\n" +
              "The program uses\n" +
              "- Qt " + root.qtVersion + ", libZ, gzstream, Manifold"
    }

    readonly property string qtVersion: "6"

    footer: Item {
        implicitHeight: 60
        BtButton {
            objectName: "shell.about.close"
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            text: qsTr("Close")
            primary: true
            onClicked: root.close()
        }
    }
}
