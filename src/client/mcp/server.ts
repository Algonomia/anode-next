/**
 * MCP Server configuration
 */

import { McpServer, ResourceTemplate } from '@modelcontextprotocol/sdk/server/mcp.js';
import { AnodeClient } from './client.js';
import { registerCatalogTools } from './tools/catalog.js';
import { registerGraphTools } from './tools/graphs.js';
import { registerEditingTools } from './tools/editing.js';
import { registerExecutionTools } from './tools/execution.js';

const GUIDE_CONTENT = `# AnodeServer MCP - Guide pour Claude

Ce serveur MCP te permet d'interagir avec AnodeServer, un moteur de graphs de traitement de données.

## Concept Global

AnodeServer est un éditeur de graphs de flux de données. Les données circulent des noeuds sources vers les noeuds de traitement.

## Types de Données

- \`int\`, \`double\`, \`string\`, \`bool\` : Scalaires
- \`csv\` : DataFrame (tableau de données)
- \`field\` : Référence à une colonne d'un CSV

## Noeuds Disponibles

### Scalaires (Entry Points - pas d'entrées)
- \`scalar/int_value\` : propriété \`_value\`, sortie \`value\`
- \`scalar/double_value\` : propriété \`_value\`, sortie \`value\`
- \`scalar/string_value\` : propriété \`_value\`, sortie \`value\`
- \`scalar/bool_value\` : propriété \`_value\`, sortie \`value\`

### Source de Données
- \`data/csv_source\` : sortie \`csv\` (DataFrame de test avec colonnes id, name, price)

### Sélection de Colonnes
- \`csv/field\` : entrée \`csv\`, propriété \`_column\`, sorties \`field\` et \`csv\`

### Mathématiques
- \`math/add\`, \`math/subtract\`, \`math/multiply\`, \`math/divide\`, \`math/modulus\`
- Entrées: \`src\` (requis), \`operand\` (requis), \`csv\` (optionnel), \`dest\` (optionnel)
- Sorties: \`result\` (scalaire), \`csv\` (DataFrame transformé)

### Chaînage d'opérations math sur CSV
- Connecter \`csv\` sortie → \`csv\` entrée du nœud suivant (transporte le DataFrame)
- Le port \`src\` reçoit toujours le \`field\` (colonne), PAS le \`result\`
- \`result\` est un scalaire (double), pas utilisable pour chaîner sur DataFrame
- Exemple (prix×5)+50: field("price") → multiply.src ET field("price") → add.src

### Agrégation
- \`aggregate/group\` : entrées \`csv\`, \`field\`, propriété \`_aggregation\` (sum/avg/min/max/first/count)

## Workflow Typique

1. \`new_graph()\` - Commencer un graph vierge
2. \`add_node(type, properties)\` - Ajouter des noeuds
3. \`connect_nodes(from_node, from_port, to_node, to_port)\` - Connecter les noeuds
4. \`create_graph(slug, name)\` - Sauvegarder le graph
5. \`execute_graph()\` - Exécuter et voir les résultats
6. \`query_dataframe(node_id, port)\` - Voir les DataFrames résultants

## Exemple: Addition de deux entiers

\`\`\`
add_node("scalar/int_value", properties: {_value: 5})  → node_1
add_node("scalar/int_value", properties: {_value: 3})  → node_2
add_node("math/add")                                   → node_3
connect_nodes("node_1", "value", "node_3", "src")
connect_nodes("node_2", "value", "node_3", "operand")
create_graph(slug: "test", name: "Test")
execute_graph()  → résultat: 8
\`\`\`

## Conventions

- Les propriétés commencent par \`_\` (ex: \`_value\`, \`_column\`)
- Port \`value\` : sortie des noeuds scalaires
- Port \`csv\` : DataFrame
- Port \`field\` : référence à une colonne
- Ports \`src\` et \`operand\` : entrées des opérations math

## Outils Disponibles

- Catalogue: \`list_nodes\`, \`get_node_info\`
- Graphs: \`list_graphs\`, \`get_graph\`, \`create_graph\`, \`save_graph\`, \`delete_graph\`, \`new_graph\`
- Édition: \`add_node\`, \`remove_node\`, \`connect_nodes\`, \`disconnect_nodes\`, \`set_property\`, \`get_node\`, \`get_current_graph\`, \`list_connections\`
- Exécution: \`execute_graph\`, \`query_dataframe\`
`;

export function createServer(baseUrl: string): McpServer {
  const server = new McpServer({
    name: 'anode-graph-editor',
    version: '1.0.0',
  });

  const client = new AnodeClient(baseUrl);

  // Register resource for the guide
  server.resource(
    'guide',
    'anode://guide',
    async (uri) => ({
      contents: [
        {
          uri: uri.href,
          mimeType: 'text/markdown',
          text: GUIDE_CONTENT,
        },
      ],
    })
  );

  // Register all tools
  registerCatalogTools(server, client);
  registerGraphTools(server, client);
  registerEditingTools(server);
  registerExecutionTools(server, client);

  return server;
}
