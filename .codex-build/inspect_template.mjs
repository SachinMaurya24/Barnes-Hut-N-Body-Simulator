import { FileBlob, PresentationFile } from "@oai/artifact-tool";

const source = "/Users/sachinmaurya/.codex/attachments/4ed2588c-a693-4ad8-a96d-1205593a917d/design-review.pptx";
const deck = await PresentationFile.importPptx(await FileBlob.load(source));
const result = await deck.inspect({
  select: "$.slides[*]",
  include: ["id", "index", "layout", "title", "elements.id", "elements.kind", "elements.text", "elements.position", "elements.style", "notes.text"],
});
console.log(result.ndjson);
console.log(JSON.stringify({ masters: deck.masters.items.map(m => ({id:m.id, name:m.name})), slideSize: deck.slideSize }, null, 2));
