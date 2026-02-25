const vscode = acquireVsCodeApi();

window.addEventListener("load", main);
window.addEventListener('message', event => { onMessage(event.data); });

const scope_function = 1;
const scope_file = 2;
const scope_closure = 4;
const scope_phpinternal = 8;

let summary_time = 0
let functions_sort = ''

/**
 * @typedef {{ file: string,fnname: string, line: number, selftime?: number, time?: number, calls?: number,}} fnRow
 */

/**
 * Array of functions flatterned.
 * @type {Array<fnRow>}
 */
let functions = [];
/** @type {number} */
let functions_scope = scope_function | scope_phpinternal;
/** @type {string} */
let functions_filter = '';

function main() {
    //
    checkHighlightElement().addEventListener('change', function () {
        vscode.postMessage({ command: 'check-highlight', data: this.checked ? true : false });
    });
    document.getElementById("functions-search").addEventListener('keyup', function (ev) {
        filterFunctions(this.value);
    })
    document.getElementById("functions-dropdown").addEventListener('change', function (ev) {
        functions_scope = parseInt(this.value);
        updateFunctions();
    })
    grid().addEventListener('dblclick', function (ev) {
        for (let element = ev.target; element; element = element.parentElement) {
            if (element.getAttribute('fnname-data')) {
                navigateFunction(element);
                break;
            }
        }
    })
    grid().addEventListener('click', function (ev) {
        for (let element = ev.target; element; element = element.parentElement) {
            let action = element.getAttribute('action-data');
            if (action == 'inspect') {
                inspectFunction(element.parentElement.parentElement)
                break;
            }
        }
    })
    document.getElementById("fnname-header").addEventListener('click', function (/*MouseEvent*/ev) {
        sortFunctionsBy('name-data', function (a, b) {
            return a.toLowerCase().localeCompare(b.toLowerCase());
        })
    })
    document.getElementById("path-header").addEventListener('click', function (/*MouseEvent*/ev) {
        sortFunctionsBy('file-data', function (a, b) {
            return a.toLowerCase().localeCompare(b.toLowerCase());
        })
    })
    document.getElementById("calls-header").addEventListener('click', function (/*MouseEvent*/ev) {
        sortFunctionsBy('calls-data', function (a, b) {
            return parseInt(b) - parseInt(a);
        })
    })
    document.getElementById("selftime-header").addEventListener('click', function (/*MouseEvent*/ev) {
        sortFunctionsBy('selftime-data', function (a, b) {
            return parseFloat(b) - parseFloat(a);
        })
    })
    document.getElementById("time-header").addEventListener('click', function (/*MouseEvent*/ev) {
        sortFunctionsBy('time-data', function (a, b) {
            return parseFloat(b) - parseFloat(a);
        })
    })
}

function onMessage(e) {
    switch (e.command) {
        case 'loading':
            loading(e.data);
            break;
        case 'summary':
            loadSummary(e.data);
            break;
        case 'functions':
            loadFunctions(e.data);
            break;
        case 'fndetails':
            loadDetails(e.data);
            break;
        case 'check-highlight':
            checkHighlightElement().setAttribute('checked', e.data ? true : false);
            break;
        case 'evaluation':
            document.getElementById('evaluation').style = e.data ? 'display:block;' : 'display:none;';
            break;
    }
}

function grid() {
    return document.getElementById('functions-grid');
}

function checkHighlightElement() {
    return document.getElementById('check-highlight');
}

function loading(e) {

    let targetId = e.target ? `loading-${e.target}` : `loading`
    let element = document.getElementById(targetId);
    if (element) {
        element.setAttribute('style', e.enabled ? 'display:block;' : 'display:none;');

        let msgelement = document.getElementById(`${targetId}-message`)
        if (msgelement) {
            msgelement.innerHTML = e.message ?? '';
        }
    }
}

function sortFunctionsBy(dataname, comparer) {
    let sort = functions_sort == `${dataname}-asc` ? -1 : +1;
    functions_sort = sort < 0 ? `${dataname}-desc` : `${dataname}-asc`;

    sortFunctions(function (a, b) {
        return comparer(a.getAttribute(dataname), b.getAttribute(dataname)) * sort;
    })
}

function sortFunctions(comparer) {
    const g = grid();
    [...g.getElementsByClassName('function-row')]
        .sort(comparer)
        .forEach(item => g.appendChild(item))
}

/** @param {HTMLElement} row */
function navigateFunction(row) {
    vscode.postMessage({
        command: 'navigate',
        data: {
            file: row.getAttribute('file-data'),
            fnname: row.getAttribute('fnname-data'),
            line: row.getAttribute('line-data'),
        }
    })
}

/** @param {fnRow[]} list */
function loadFunctions(list) {
    functions = list;
    updateFunctions();
}

/**
 * @param {"function"|"closure"|"file"|"php::internal"} scope 
 */
function updateFunctions() {
    const g = grid();

    // remove previous rows
    let toremove = g.getElementsByClassName('function-row');
    for (let i = toremove.length - 1; i >= 0; i--) {
        g.removeChild(toremove[i]);
    }

    // add new rows
    for (const fn of functions) {
        let fnname = fn.fnname;
        let obj = sanitizeFnName(fnname);
        if ((functions_scope & obj.scope) == 0) {
            continue;
        }

        if (fn.time < 0.00001/*0.01ms*/) { // ignore functions that take ~0.00 ms
            continue;
        }

        let row = document.createElement('vscode-data-grid-row');
        row.setAttribute('class', `function-row`);
        row.setAttribute('fnname-data', fnname); // text used for filtering
        row.setAttribute('name-data', obj.name); // text used for sorting
        row.setAttribute('calls-data', fn.calls);
        row.setAttribute('selftime-data', fn.selftime);
        row.setAttribute('time-data', fn.time);
        row.setAttribute('file-data', fn.file);
        row.setAttribute('line-data', fn.line);

        //
        row.innerHTML =
            `
<vscode-data-grid-cell grid-column="1" class="function-fnname">
    <span class="codicon ${obj.codicon}"></span>
    ${escapeHTML(obj.name)}
    <span class="action codicon codicon-inspect" action-data="inspect"></span>
</vscode-data-grid-cell>
<vscode-data-grid-cell grid-column="2" class="text-right">${fn.calls}</vscode-data-grid-cell>
<vscode-data-grid-cell grid-column="3" class="text-right">${timeHtml(fn.selftime)}</vscode-data-grid-cell>
<vscode-data-grid-cell grid-column="4" class="text-right">${timeHtml(fn.time)}</vscode-data-grid-cell>
<vscode-data-grid-cell grid-column="5" class="function-file">${escapeHTML(fn.file)}</vscode-data-grid-cell>
`;
        g.appendChild(row)
    }

    // filter, if set
    if (functions_filter && functions_filter.length) {
        filterFunctions(functions_filter);
    }
}

const fnname_file_prefixes = ['include::', 'include_once::', 'require::', 'require_once::'];

/**
 * @param {string} fnname
 */
function sanitizeFnName(fnname) {
    const phpinternal = 'php::';
    const main = '{main}';
    const closure = '{closure:'; // inside the string

    // php::
    if (fnname.startsWith(phpinternal)) {
        return {
            "name": `${fnname.substring(phpinternal.length).replace('->', '::')}()`,
            "codicon": 'codicon-symbol-keyword',
            "scope": scope_phpinternal,
        };
    }

    // {main}
    if (fnname == main) {
        return {
            "name": main,
            "codicon": 'codicon-symbol-file',
            "scope": scope_file,
        };
    }

    // require::
    for (const prefix of fnname_file_prefixes) {
        if (fnname.startsWith(prefix)) {
            let file = fnname.substring(prefix.length);
            let separator = file.lastIndexOf('/');
            if (separator < 0) separator = file.lastIndexOf('\\');
            return {
                "name": separator >= 0 ? file.substring(separator + 1) : file,
                "codicon": 'codicon-symbol-file',
                "scope": scope_file,
            };
        }
    }

    // ...{closure:file:line-line}
    let closureat = fnname.indexOf(closure);
    if (closureat >= 0) {
        return {
            "name": '{closure}',
            "codicon": 'codicon-symbol-misc',
            "scope": scope_closure,
        }
    }

    // trim namespace
    let nsseparator = fnname.lastIndexOf('\\');
    if (nsseparator >= 0) {
        fnname = fnname.substring(nsseparator + 1);
    }

    return {
        "name": `${fnname.replace('->', '::')}()`,
        "codicon": 'codicon-symbol-method',
        "scope": scope_function,
    }
}

/** @param {string} unsafeText */
function escapeHTML(unsafeText) {
    let div = document.createElement('div');
    div.innerText = unsafeText;
    return div.innerHTML;
}

/**
 * @param {string} filter 
 */
function filterFunctions(filter) {
    const filterlower = functions_filter = filter.toLowerCase();
    for (const element of grid().getElementsByClassName('function-row')) {
        let fnname = element.getAttribute('fnname-data')
        if (fnname) {
            if (fnname.toLowerCase().includes(filterlower))
                element.classList.remove('hidden');
            else
                element.classList.add('hidden');
        }
    }
}

/** @param {HTMLElement} row Element with "fnname-data" attribute. */
function inspectFunction(row) {
    vscode.postMessage({
        command: 'inspect',
        data: {
            file: row.getAttribute('file-data'),
            fnname: row.getAttribute('fnname-data'),
        }
    })

    // switch to tab-2
    document.getElementById('panels').setAttribute('activeid', 'tab-2');
}

/**
 * @param {{callers: fnRow[], self: fnRow, called: fnRow[]}} data 
 */
function loadDetails(data) {
    // clear existing
    for (const content of document.getElementsByClassName('details-content')) {
        content.innerHTML = '';
    }

    // add data
    let container = document.getElementById('details-self').getElementsByClassName('details-content')[0];
    addDetails(data.self, container, true);

    container = document.getElementById('details-caller').getElementsByClassName('details-content')[0];
    for (const fn of data.callers) {
        addDetails(fn, container);
    }

    container = document.getElementById('details-called').getElementsByClassName('details-content')[0];
    for (const fn of data.called) {
        addDetails(fn, container);
    }

    // insert dummy text if empty
    for (const content of document.getElementsByClassName('details-content')) {
        if (!content.childElementCount) {
            content.innerHTML = `<span class="details-empty">no calls</span>`;
        }
    }
}

/**
 * @param {fnRow} fn
 * @param {HTMLElement} container
 */
function addDetails(fn, container, addmore) {
    if (!fn || !container) return;
    let obj = sanitizeFnName(fn.fnname);

    const createDetails = function (name, time, ismore) {
        let item = document.createElement('div');
        item.classList.add('details-item')
        if (ismore)
            item.classList.add('details-item-more');
        item.innerHTML = `
<div class="details-item-name">${escapeHTML(name)}</div>
<div class="details-item-time">${escapeHTML(time)}</div>
`;
        container.appendChild(item);
        return item;
    }

    let row = createDetails(
        obj.name,
        `${timeHtml(fn.time)}ms (${summary_time > 0 ? asPercent(fn.time / summary_time) : '?'}%)`
    );
    row.setAttribute('fnname-data', fn.fnname);
    row.addEventListener('click', function (ev) {
        inspectFunction(this);
    });
    
    if (addmore) {

        createDetails(
            'Self Time:',
            `${timeHtml(fn.selftime)}ms (${summary_time > 0 ? asPercent(fn.selftime / summary_time) : '?'}%)`,
            true);

        // createDetails(
        //     '',
        //     `<a href="#">Go To</a>`);
    }
}

function loadSummary(summary) {
    document.getElementById('summary-time').innerHTML = `<b>Time:</b> ${timeHtml(summary_time = summary.time)} (ms)`;
    document.getElementById('summary-memory').innerHTML = `<b>Memory:</b> ${summary.memory.toLocaleString('en', { useGrouping: true })} (bytes)`;
}

function timeHtml(seconds) {
    return `${(seconds * 1000.0)
        .toLocaleString('en', { useGrouping: true, maximumFractionDigits: 2, minimumFractionDigits: 2 })
        .replace(',', ' ')}`
}
function asPercent(value) {
    return `${(value * 100.0)
        .toLocaleString('en', { useGrouping: true, maximumFractionDigits: 2, minimumFractionDigits: 2 })
        .replace(',', ' ')}`
}
