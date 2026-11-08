// Reads facts gathered by the workflow (JSON file) and prints the decision.
// Exit 0 = merge, 1 = do not merge (reasons printed), 2 = bad input.
import fs from 'node:fs';
import { evaluate } from './automerge.mjs';

try {
  const facts = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
  const result = evaluate({ ...facts, allowUiWithoutReview: process.env.MATRIX_AUTOMERGE_UI !== 'false' });
  console.log(JSON.stringify(result, null, 2));
  process.exit(result.merge ? 0 : 1);
} catch (error) {
  console.error(`Cannot evaluate: ${error.message}`);
  process.exit(2);
}
