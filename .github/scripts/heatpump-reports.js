// Handles issues from the "Add my heat pump model" and "Share my topic values" forms.
// Called from .github/workflows/heatpump-reports.yml through actions/github-script.
//
// - Topic values (from either form) are stored in the branch "model-reports": one JSON
//   file per issue (reports/<model bytes>/issue-<number>.json), never changed afterwards,
//   plus a summary per model (models/<model bytes>.json) and an overview (README.md)
//   that are rebuilt from the report files each time.
// - "Add my heat pump model": if the model bytes are already in HeatPumpType.md, the
//   issue is closed with a comment. Otherwise a pull request adds a row to
//   HeatPumpType.md and closes the issue when it is merged.
// - "Share my topic values": the issue is closed after the values are stored.
//
// The issue text is untrusted. It is only parsed here and never passed to a shell.

const MODEL_LABEL = 'heat pump model';
const VALUES_LABEL = 'topic values';
const REPORTS_BRANCH = 'model-reports';
const TYPE_FILE = 'HeatPumpType.md';

// Splits the markdown that GitHub creates from an issue form into { heading: value }.
function parseForm(body) {
  const out = {};
  const parts = ('\n' + (body || '').replace(/\r/g, '')).split(/\n### /);
  for (const part of parts.slice(1)) {
    const nl = part.indexOf('\n');
    const head = (nl < 0 ? part : part.slice(0, nl)).trim();
    let val = nl < 0 ? '' : part.slice(nl + 1).trim();
    val = val.replace(/^```[a-z]*\n?/, '').replace(/\n?```$/, '').trim();
    if (val === '_No response_') val = '';
    out[head] = val;
  }
  return out;
}

// "12 d7 0c ..." -> "12 D7 0C ...", or null if it is not ten hex bytes.
function normBytes(s) {
  const hex = String(s || '').replace(/[^0-9a-fA-F]/g, '').toUpperCase();
  if (hex.length !== 20) return null;
  if (/^0+$/.test(hex)) return null;
  return hex.match(/../g).join(' ');
}

// A short single-line cell for a markdown table.
function cell(s, max) {
  return String(s || '').replace(/[|`\r\n<>]/g, ' ').replace(/\s+/g, ' ').trim().slice(0, max || 60);
}

// Rows of the model table: { line, nr, bytes, cols }.
function tableRows(md) {
  const rows = [];
  md.split('\n').forEach((l, i) => {
    const c = l.split('|');
    if (c.length < 4 || !/^\s*\d+\s*$/.test(c[1])) return;
    const b = normBytes(c[2]);
    if (b) rows.push({ line: i, nr: parseInt(c[1], 10), bytes: b, cols: c.map(x => x.trim()) });
  });
  return rows;
}

// Only lines like TOP12=34, XTOP0=..., OPT3=... are kept.
function cleanValues(s) {
  return String(s || '').split('\n').map(l => l.trim())
    .filter(l => /^(X?TOP|OPT)\d{1,3}=[^\r\n]{0,60}$/.test(l)).slice(0, 400);
}

function kindOf(issue, f) {
  const labels = (issue.labels || []).map(l => (typeof l === 'string' ? l : l.name));
  if (labels.includes(MODEL_LABEL) || 'Power supply' in f) return 'model';
  if (labels.includes(VALUES_LABEL) || 'Topic values' in f) return 'values';
  return null;
}

function buildRow(nr, bytes, f) {
  return '|' + nr + ' | ' + bytes + ' | ' + cell(f['Indoor unit']) + ' | ' + cell(f['Outdoor unit']) +
    ' | ' + (cell(f['Kit name']) || '-') + ' | ' + (cell(f['Power in kW'], 10) || '?') +
    ' | ' + (cell(f['Power supply'], 10).replace('Not known', '?') || '?') +
    ' | ' + (cell(f['Type']) || '?') + ' |';
}

function insertRow(md, row) {
  const rows = tableRows(md);
  const lines = md.split('\n');
  lines.splice(rows[rows.length - 1].line + 1, 0, row);
  return lines.join('\n');
}

// Values that often mean "no sensor" or "not used". Not verified for every topic.
const SUSPECT = ['-78', '-128', '-200'];

function reportJson(issue, bytes, f, values) {
  const v = {};
  values.forEach(l => { const i = l.indexOf('='); v[l.slice(0, i)] = l.slice(i + 1); });
  return {
    schema: 1,
    issue: issue.number,
    created_at: issue.created_at,
    model_bytes: bytes,
    indoor_unit: cell(f['Indoor unit']),
    outdoor_unit: cell(f['Outdoor unit']),
    series: cell(f['Series'], 20).replace('Not known', ''),
    version: cell(f['HeishaMon firmware version'], 40),
    board: cell(f['Board'], 40),
    values: v,
    raw: null, // reserved for the raw datagrams (hex), see the follow-up issue
  };
}

function distinct(list) { return [...new Set(list.filter(x => x))].sort(); }

// Summary of all reports of one model. Built from the report files every time.
function summarize(bytes, reports) {
  const topics = {};
  reports.forEach(r => Object.entries(r.values || {}).forEach(([t, val]) => {
    const e = topics[t] || (topics[t] = { reports: 0, values: {} });
    e.reports++;
    if (val in e.values || Object.keys(e.values).length < 20) e.values[val] = (e.values[val] || 0) + 1;
  }));
  const order = t => { const m = t.match(/^(X?TOP|OPT)(\d+)$/); return m ? ({ TOP: 0, XTOP: 1, OPT: 2 }[m[1]] * 1000 + +m[2]) : 9999; };
  const sorted = {};
  Object.keys(topics).sort((x, y) => order(x) - order(y)).forEach(t => { sorted[t] = topics[t]; });
  return {
    schema: 1,
    model_bytes: bytes,
    indoor_units: distinct(reports.map(r => r.indoor_unit)),
    outdoor_units: distinct(reports.map(r => r.outdoor_unit)),
    series: distinct(reports.map(r => r.series)),
    reports: reports.length,
    issues: reports.map(r => r.issue).sort((x, y) => x - y),
    versions: distinct(reports.map(r => r.version)),
    last_report: reports.map(r => r.created_at).sort().pop(),
    topics: sorted,
  };
}

function suspectTopics(sum) {
  return Object.keys(sum.topics).filter(t => Object.keys(sum.topics[t].values).every(v => SUSPECT.includes(v)));
}

function overviewMd(sums) {
  const lines = [
    '# Topic values per heat pump model',
    '',
    'Generated by the "Heat pump model and topic value reports" workflow from the files in `reports/`. Do not edit by hand.',
    '',
    '- `reports/<model bytes>/issue-<n>.json`: one file per report, never changed afterwards',
    '- `models/<model bytes>.json`: summary per model, rebuilt from the reports',
    '',
    '"Suspected no sensor" counts topics whose reported values are only ' + SUSPECT.join(', ') +
      '. These values often mean that a sensor or function is not present; this is not verified for every topic.',
    '',
    '| Model bytes | Indoor unit | Outdoor unit | Reports | Topics | Suspected no sensor |',
    '| --- | --- | --- | --- | --- | --- |',
  ];
  sums.slice().sort((x, y) => x.model_bytes.localeCompare(y.model_bytes)).forEach(m => {
    const key = m.model_bytes.replace(/ /g, '');
    lines.push('| [' + m.model_bytes + '](models/' + key + '.json) | ' + (m.indoor_units.join(', ') || '?') + ' | ' +
      (m.outdoor_units.join(', ') || '?') + ' | ' + m.reports + ' | ' + Object.keys(m.topics).length + ' | ' + suspectTopics(m).length + ' |');
  });
  return lines.join('\n') + '\n';
}

const json = o => JSON.stringify(o, null, 1) + '\n';

async function readFile(github, owner, repo, path, ref) {
  const d = (await github.rest.repos.getContent({ owner, repo, path, ref })).data;
  return Buffer.from(d.content, 'base64').toString('utf8');
}

async function readJsonDir(github, owner, repo, path, ref) {
  let list;
  try {
    list = (await github.rest.repos.getContent({ owner, repo, path, ref })).data;
  } catch (e) {
    if (e.status === 404) return [];
    throw e;
  }
  const out = [];
  for (const it of (Array.isArray(list) ? list : [])) {
    if (it.type !== 'file' || !it.name.endsWith('.json')) continue;
    try { out.push(JSON.parse(await readFile(github, owner, repo, it.path, ref))); } catch (e) { if (e instanceof SyntaxError) continue; throw e; }
  }
  return out;
}

// Head commit of the reports branch; creates the branch without history if it is missing.
async function reportsHead(github, owner, repo) {
  try {
    return (await github.rest.git.getRef({ owner, repo, ref: 'heads/' + REPORTS_BRANCH })).data.object.sha;
  } catch (e) {
    if (e.status !== 404) throw e;
  }
  const tree = await github.rest.git.createTree({ owner, repo,
    tree: [{ path: 'README.md', mode: '100644', type: 'blob', content: overviewMd([]) }] });
  const commit = await github.rest.git.createCommit({ owner, repo,
    message: 'Start the model-reports branch', tree: tree.data.sha, parents: [] });
  try {
    await github.rest.git.createRef({ owner, repo, ref: 'refs/heads/' + REPORTS_BRANCH, sha: commit.data.sha });
    return commit.data.sha;
  } catch (e) {
    if (e.status !== 422) throw e; // created by a parallel run in the meantime
    return (await github.rest.git.getRef({ owner, repo, ref: 'heads/' + REPORTS_BRANCH })).data.object.sha;
  }
}

// Writes the report, the model summary and the overview in one commit. Runs for other
// reports may write at the same time; then the branch has moved on, the update is not a
// fast-forward and everything is read and built again.
async function storeReport(github, owner, repo, report) {
  const key = report.model_bytes.replace(/ /g, '');
  const reportPath = 'reports/' + key + '/issue-' + report.issue + '.json';
  for (let attempt = 1; attempt <= 6; attempt++) {
    const head = await reportsHead(github, owner, repo);
    const reports = (await readJsonDir(github, owner, repo, 'reports/' + key, head))
      .filter(r => r.issue !== report.issue).concat([report]);
    const sum = summarize(report.model_bytes, reports);
    const sums = (await readJsonDir(github, owner, repo, 'models', head))
      .filter(m => m.model_bytes !== report.model_bytes).concat([sum]);
    const files = { [reportPath]: json(report), ['models/' + key + '.json']: json(sum), 'README.md': overviewMd(sums) };
    const base = (await github.rest.git.getCommit({ owner, repo, commit_sha: head })).data.tree.sha;
    const tree = await github.rest.git.createTree({ owner, repo, base_tree: base,
      tree: Object.entries(files).map(([path, content]) => ({ path, mode: '100644', type: 'blob', content })) });
    const commit = await github.rest.git.createCommit({ owner, repo,
      message: 'Add the report from issue #' + report.issue + ' (' + report.model_bytes + ')', tree: tree.data.sha, parents: [head] });
    try {
      await github.rest.git.updateRef({ owner, repo, ref: 'heads/' + REPORTS_BRANCH, sha: commit.data.sha, force: false });
      return { path: reportPath, summary: 'models/' + key + '.json', reports: sum.reports };
    } catch (e) {
      if (e.status !== 422 && e.status !== 409) throw e;
      await new Promise(r => setTimeout(r, 500 * attempt + Math.floor(Math.random() * 500)));
    }
  }
  throw new Error('The model-reports branch kept changing; the report was not stored.');
}

module.exports = async ({ github, context }) => {
  const issue = context.payload.issue;
  const { owner, repo } = context.repo;
  const f = parseForm(issue.body);
  const kind = kindOf(issue, f);
  if (!kind) return 'not a form issue';

  const say = body => github.rest.issues.createComment({ owner, repo, issue_number: issue.number, body });
  const close = reason => github.rest.issues.update({ owner, repo, issue_number: issue.number,
    state: 'closed', state_reason: reason || 'completed' });

  const bytes = normBytes(f['Model bytes (TOP92)']);
  if (!bytes) {
    await say('The model bytes could not be read. They should be ten hex bytes like `12 D7 0C 98 14 35 94 0D 83 10`. Please edit the issue; a maintainer will have a look.');
    return 'bad model bytes';
  }

  const def = (await github.rest.repos.get({ owner, repo })).data.default_branch;
  const typeFile = (await github.rest.repos.getContent({ owner, repo, path: TYPE_FILE, ref: def })).data;
  const md = Buffer.from(typeFile.content, 'base64').toString('utf8');
  const rows = tableRows(md);
  const known = rows.find(r => r.bytes === bytes);
  const notes = [];

  const values = cleanValues(f['Topic values (optional)'] || f['Topic values']);
  if (values.length) {
    const st = await storeReport(github, owner, repo, reportJson(issue, bytes, f, values));
    const link = p => '[`' + p + '`](https://github.com/' + owner + '/' + repo + '/blob/' + REPORTS_BRANCH + '/' + p + ')';
    notes.push('Thanks, the ' + values.length + ' topic values are stored in ' + link(st.path) +
      '. The summary for this model (' + st.reports + (st.reports === 1 ? ' report' : ' reports') + ') is in ' + link(st.summary) + '.');
  }

  if (kind === 'values') {
    if (!values.length) notes.push('No topic values were found in this issue.');
    await say(notes.join('\n\n'));
    await close('completed');
    return 'values';
  }

  if (known) {
    notes.unshift('This model is already listed in ' + TYPE_FILE + ' as number ' + known.nr + ': ' +
      known.cols[3] + ' / ' + known.cols[4] + '. Nothing to add.');
    await say(notes.join('\n\n'));
    await close('completed');
    return 'known';
  }

  if (!cell(f['Indoor unit']) || !cell(f['Outdoor unit'])) {
    notes.unshift('The indoor or the outdoor unit is missing. Please edit the issue and add both from the type plates; a maintainer will have a look.');
    await say(notes.join('\n\n'));
    return 'missing units';
  }

  const key = bytes.replace(/ /g, '');
  const open = await github.paginate(github.rest.pulls.list, { owner, repo, state: 'open', per_page: 100 });
  const dup = open.find(p => p.head && p.head.ref && p.head.ref.startsWith('model/' + key));
  if (dup) {
    notes.unshift('These model bytes are already proposed in #' + dup.number + '.');
    await say(notes.join('\n\n'));
    await close('not_planned');
    return 'duplicate';
  }

  const nr = Math.max(...rows.map(r => r.nr)) + 1;
  const row = buildRow(nr, bytes, f);
  const branch = 'model/' + key + '-issue' + issue.number;
  const head = (await github.rest.git.getRef({ owner, repo, ref: 'heads/' + def })).data.object.sha;
  await github.rest.git.createRef({ owner, repo, ref: 'refs/heads/' + branch, sha: head });
  await github.rest.repos.createOrUpdateFileContents({ owner, repo, path: TYPE_FILE, branch,
    message: 'Add heat pump model ' + bytes + ' (#' + issue.number + ')',
    content: Buffer.from(insertRow(md, row), 'utf8').toString('base64'), sha: typeFile.sha });
  const pr = await github.rest.pulls.create({ owner, repo, base: def, head: branch,
    title: 'Add heat pump model ' + cell(f['Indoor unit']) + ' / ' + cell(f['Outdoor unit']),
    body: 'Closes #' + issue.number + '\n\nAdded automatically from the "Add my heat pump model" form. Please check the row against the type plates before merging:\n\n```\n' + row + '\n```\n' });
  notes.unshift('Thanks, this is a new model. #' + pr.data.number + ' adds it to ' + TYPE_FILE + '; this issue is closed when that is merged.');
  await say(notes.join('\n\n'));
  return 'new model';
};

module.exports.parseForm = parseForm;
module.exports.normBytes = normBytes;
module.exports.tableRows = tableRows;
module.exports.buildRow = buildRow;
module.exports.insertRow = insertRow;
module.exports.cleanValues = cleanValues;
module.exports.summarize = summarize;
module.exports.overviewMd = overviewMd;
