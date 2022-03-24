using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Editor;

namespace Game.Player;

[Component]
[EditorIcon(FontAwesome.FontAwesomeIcons.HandPointDown)]
[EditorFriendlyName("Spawn Point")]
[Gizmo("Gizmos/SpawnPoint.json")]
public class SpawnPoint
{
}
