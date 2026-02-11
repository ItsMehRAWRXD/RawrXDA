// @ts-check

(function () {
	// @ts-ignore
	const vscode = acquireVsCodeApi();

	const canvasEl = document.getElementById('simulator-canvas');
	if (!(canvasEl instanceof HTMLCanvasElement)) {
		return;
	}
	/** @type {HTMLCanvasElement} */
	const canvas = canvasEl;

	const ctxMaybe = canvas.getContext('2d');
	if (!ctxMaybe) {
		return;
	}
	/** @type {CanvasRenderingContext2D} */
	const ctx = ctxMaybe;
	/** @type {HTMLElement} */
	const loadingElMaybe = document.getElementById('loading');
	if (!(loadingElMaybe instanceof HTMLElement)) {
		return;
	}
	const loadingEl = loadingElMaybe;
	/** @type {HTMLElement} */
	const wrapperElMaybe = document.getElementById('simulator-wrapper');
	if (!(wrapperElMaybe instanceof HTMLElement)) {
		return;
	}
	const wrapperEl = wrapperElMaybe;

	// State
	let hasReceivedFrame = false;

	// Frame queue for smooth rendering
	/** @type {string | null} */
	let pendingFrame = null;
	let isRendering = false;

	// Gesture tracking
	/** @type {{ x: number; y: number; time: number } | null} */
	let pointerDownStart = null;
	let isPointerDown = false;
	const TAP_THRESHOLD_MS = 500; // Allow longer taps
	const SWIPE_THRESHOLD_PX = 15; // Slightly lower threshold for better tap detection
	const LONG_PRESS_THRESHOLD_MS = 500;
	/** @type {ReturnType<typeof setTimeout> | null} */
	let longPressTimer = null;

	// Set tabindex early so canvas can receive focus
	canvas.setAttribute('tabindex', '0');

	// Reuse a single Image object to avoid GC pressure
	const frameImage = new Image();
	let lastSentFrameSize = { width: 0, height: 0 };

	function renderFrame() {
		if (!pendingFrame || isRendering) {
			return;
		}

		isRendering = true;
		const frameData = pendingFrame;
		pendingFrame = null;

		frameImage.onload = function () {
			// Resize canvas to match frame aspect ratio (only if changed)
			if (canvas.width !== frameImage.width || canvas.height !== frameImage.height) {
				canvas.width = frameImage.width;
				canvas.height = frameImage.height;
			}

			// Inform extension of current frame dimensions so it can map input coordinates.
			// (This avoids guessing simulator screen bounds via accessibility.)
			if (lastSentFrameSize.width !== canvas.width || lastSentFrameSize.height !== canvas.height) {
				lastSentFrameSize = { width: canvas.width, height: canvas.height };
				vscode.postMessage({
					type: 'frameInfo',
					width: canvas.width,
					height: canvas.height,
				});
			}

			// Use requestAnimationFrame for smooth rendering
			requestAnimationFrame(function () {
				ctx.drawImage(frameImage, 0, 0);
				isRendering = false;

				// Render next pending frame if any
				if (pendingFrame) {
					renderFrame();
				}
			});

			if (!hasReceivedFrame) {
				hasReceivedFrame = true;
				loadingEl.classList.add('hidden');
				canvas.focus();
			}
		};

		frameImage.onerror = function () {
			isRendering = false;
			// Try next frame if any
			if (pendingFrame) {
				renderFrame();
			}
		};

		frameImage.src = frameData;
	}

	// Message handling from extension
	window.addEventListener('message', function (event) {
		const message = event.data;
		switch (message.type) {
			case 'frame':
				// Queue the frame (drop old pending frame if any - we only care about latest)
				pendingFrame = `data:${message.mimeType};base64,${message.data}`;
				renderFrame();
				break;
		}
	});

	// Helper: Get normalized coordinates (0-1) from pointer event
	function getNormalizedCoords(event) {
		const rect = canvas.getBoundingClientRect();
		const scaleX = canvas.width / rect.width;
		const scaleY = canvas.height / rect.height;
		const canvasX = (event.clientX - rect.left) * scaleX;
		const canvasY = (event.clientY - rect.top) * scaleY;
		return {
			x: canvasX / canvas.width,
			y: canvasY / canvas.height,
		};
	}

	// Helper: Show touch feedback
	function showTouchFeedback(clientX, clientY) {
		const indicator = document.createElement('div');
		indicator.className = 'touch-indicator';
		indicator.style.left = clientX + 'px';
		indicator.style.top = clientY + 'px';
		document.body.appendChild(indicator);
		setTimeout(function () {
			indicator.remove();
		}, 300);
	}

	// Pointer events for touch/mouse handling
	canvas.addEventListener('pointerdown', function (event) {
		event.preventDefault();
		canvas.setPointerCapture(event.pointerId);
		canvas.focus();

		isPointerDown = true;
		const coords = getNormalizedCoords(event);
		pointerDownStart = {
			x: coords.x,
			y: coords.y,
			time: Date.now(),
		};

		// Start long press timer
		longPressTimer = setTimeout(function () {
			if (isPointerDown && pointerDownStart) {
				// Long press detected
				const holdMs = Date.now() - pointerDownStart.time;
				vscode.postMessage({
					type: 'tap',
					x: pointerDownStart.x,
					y: pointerDownStart.y,
					holdMs: holdMs,
				});
				showTouchFeedback(event.clientX, event.clientY);
				pointerDownStart = null; // Prevent additional tap on pointer up
			}
		}, LONG_PRESS_THRESHOLD_MS);
	});

	canvas.addEventListener('pointermove', function (event) {
		if (!isPointerDown || !pointerDownStart) {
			return;
		}

		const coords = getNormalizedCoords(event);
		const deltaX = (coords.x - pointerDownStart.x) * canvas.width;
		const deltaY = (coords.y - pointerDownStart.y) * canvas.height;
		const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);

		// If moved beyond threshold, cancel long press
		if (distance > SWIPE_THRESHOLD_PX && longPressTimer) {
			clearTimeout(longPressTimer);
			longPressTimer = null;
		}
	});

	canvas.addEventListener('pointerup', function (event) {
		event.preventDefault();
		canvas.releasePointerCapture(event.pointerId);

		// Clear long press timer
		if (longPressTimer) {
			clearTimeout(longPressTimer);
			longPressTimer = null;
		}

		if (!isPointerDown || !pointerDownStart) {
			isPointerDown = false;
			return;
		}

		const coords = getNormalizedCoords(event);
		const deltaX = (coords.x - pointerDownStart.x) * canvas.width;
		const deltaY = (coords.y - pointerDownStart.y) * canvas.height;
		const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
		const duration = Date.now() - pointerDownStart.time;

		if (distance < SWIPE_THRESHOLD_PX) {
			// Tap - if we didn't move much, it's a tap regardless of duration
			// (long press is handled separately by the timer)
			vscode.postMessage({
				type: 'tap',
				x: pointerDownStart.x,
				y: pointerDownStart.y,
			});
			showTouchFeedback(event.clientX, event.clientY);
		} else {
			// Swipe - moved beyond threshold
			vscode.postMessage({
				type: 'swipe',
				startX: pointerDownStart.x,
				startY: pointerDownStart.y,
				endX: coords.x,
				endY: coords.y,
				durationMs: 100,
			});
		}

		isPointerDown = false;
		pointerDownStart = null;
	});

	canvas.addEventListener('pointercancel', function (event) {
		if (longPressTimer) {
			clearTimeout(longPressTimer);
			longPressTimer = null;
		}
		isPointerDown = false;
		pointerDownStart = null;
	});

	// Keyboard handling
	canvas.addEventListener('keydown', function (event) {
		// Prevent default for navigation keys
		if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Tab', 'Enter', 'Backspace', 'Delete', 'Escape'].includes(event.key)) {
			event.preventDefault();
		}

		// Don't send modifier-only key presses
		if (['Shift', 'Control', 'Alt', 'Meta'].includes(event.key)) {
			return;
		}

		vscode.postMessage({
			type: 'key',
			key: event.key,
			code: event.code,
			shiftKey: event.shiftKey,
			ctrlKey: event.ctrlKey,
			altKey: event.altKey,
		});
	});

	// Prevent context menu
	canvas.addEventListener('contextmenu', function (event) {
		event.preventDefault();
	});

	// Handle paste for text input
	document.addEventListener('paste', function (event) {
		const text = event.clipboardData?.getData('text');
		if (text && document.activeElement === canvas) {
			event.preventDefault();
			vscode.postMessage({
				type: 'text',
				text: text,
			});
		}
	});

	// Home button handler
	const homeBtn = document.getElementById('home-btn');
	if (homeBtn) {
		homeBtn.addEventListener('click', function (event) {
			event.preventDefault();
			vscode.postMessage({ type: 'home' });
		});
	}

	// Notify extension that webview is ready
	vscode.postMessage({ type: 'ready' });
})();
