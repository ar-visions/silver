const vscode = require('vscode');

function activate(context) {
    const out = vscode.window.createOutputChannel('Silver');
    out.appendLine('[ag] extension activated');
    out.show();

    // test: regular hover works outside debug too
    context.subscriptions.push(
        vscode.languages.registerHoverProvider('ag', {
            provideHover(document, position) {
                const line = document.lineAt(position).text;
                let start = position.character;
                while (start > 0 && /[\w]/.test(line[start - 1])) start--;
                let end = position.character;
                while (end < line.length && /[\w]/.test(line[end])) end++;
                const word = line.substring(start, end);
                if (word) console.log('[ag-hover-test] ' + word);
                return null;
            }
        })
    );

    let cachedSelfRef = 0;
    let cachedFrameId = null;

    context.subscriptions.push(
        vscode.debug.registerDebugAdapterTrackerFactory('lldb', {
            createDebugAdapterTracker(session) {
                return {
                    onDidSendMessage(msg) {
                        // on stop: evaluate *a and cache the variablesReference
                        if (msg.type === 'event' && msg.event === 'stopped') {
                            cachedSelfRef = 0;
                            (async () => {
                                try {
                                    const threads = await session.customRequest('threads');
                                    if (!threads.threads.length) return;
                                    const stack = await session.customRequest('stackTrace', {
                                        threadId: threads.threads[0].id,
                                        startFrame: 0, levels: 1
                                    });
                                    if (!stack.stackFrames.length) return;
                                    cachedFrameId = stack.stackFrames[0].id;
                                    const r = await session.customRequest('evaluate', {
                                        expression: '*a',
                                        frameId: cachedFrameId,
                                        context: 'repl'
                                    });
                                    if (r && r.variablesReference > 0)
                                        cachedSelfRef = r.variablesReference;
                                } catch {}
                            })();
                        }

                        // inject scope into scopes response
                        if (msg.type === 'response' && msg.command === 'scopes' &&
                            msg.success && msg.body && msg.body.scopes &&
                            cachedSelfRef > 0) {
                            msg.body.scopes.push({
                                name: 'Object (a)',
                                variablesReference: cachedSelfRef,
                                expensive: false
                            });
                        }
                    }
                };
            }
        })
    );

    // hover
    context.subscriptions.push(
        vscode.languages.registerEvaluatableExpressionProvider('ag', {
            async provideEvaluatableExpression(document, position) {
                const line = document.lineAt(position).text;
                const col  = position.character;

                let start = col;
                while (start > 0 && /[\w]/.test(line[start - 1])) start--;
                let end = col;
                while (end < line.length && /[\w]/.test(line[end])) end++;
                if (start >= end) return undefined;

                const word = line.substring(start, end);
                if (!word.match(/^[a-zA-Z_]/)) return undefined;

                let exprStart = start, s = start;
                while (s > 0) {
                    if (line[s - 1] === '.') {
                        s--; let ws = s;
                        while (ws > 0 && /[\w]/.test(line[ws - 1])) ws--;
                        exprStart = ws; s = ws;
                    } else if (s >= 2 && line[s - 1] === '>' && line[s - 2] === '-') {
                        s -= 2; let ws = s;
                        while (ws > 0 && /[\w]/.test(line[ws - 1])) ws--;
                        exprStart = ws; s = ws;
                    } else break;
                }

                const range = new vscode.Range(position.line, exprStart, position.line, end);
                if (exprStart < start)
                    return new vscode.EvaluatableExpression(range, line.substring(exprStart, end));

                // bare identifier — try local, fall back to a->member
                const session = vscode.debug.activeDebugSession;
                if (session && cachedFrameId !== null) {
                    try {
                        await session.customRequest('evaluate', {
                            expression: word, frameId: cachedFrameId, context: 'hover'
                        });
                        return new vscode.EvaluatableExpression(range, word);
                    } catch {
                        return new vscode.EvaluatableExpression(range, 'a->' + word);
                    }
                }

                return new vscode.EvaluatableExpression(range, word);
            }
        })
    );
}

function deactivate() {}

module.exports = { activate, deactivate };
