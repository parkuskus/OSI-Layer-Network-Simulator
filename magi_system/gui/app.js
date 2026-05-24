(function () {
  "use strict";

  var fallbackTopology = {
    hosts: [
      { name: "H1", ip_address: "192.168.1.10/24", default_gateway: "192.168.1.1" },
      { name: "H2", ip_address: "192.168.1.11/24", default_gateway: "192.168.1.1" }
    ],
    switches: [
      { name: "SW1", num_ports: 8, vlans: [] }
    ],
    routers: [
      { name: "R1", interfaces: [], routing_table: [] }
    ],
    links: [
      { endpoints: ["H1", "SW1:1"], delay: 5 },
      { endpoints: ["H2", "SW1:2"], delay: 5 }
    ]
  };

  var state = {
    topology: fallbackTopology,
    nodes: [],
    links: [],
    positions: {},
    manualPositions: {},
    selectedNode: null,
    selectedTab: "overview",
    packetCount: 0,
    zoom: 1,
    drag: null,
    apiAvailable: false,
    mode: "select",
    selectedToolType: "switch",
    pendingLinkSource: null,
    pendingPlacements: {},
    lastCommand: "",
    lastCommandOutput: "No CLI command executed yet.",
    logFilter: "all",
    commandBatchRunning: false,
    canvasSize: null,
    inspectorWidth: 338,
    inspectorResize: null,
    linkDraft: {
      delay: 5,
      mtu: 1500
    },
    source: "fallback topology"
  };

  var els = {};

  document.addEventListener("DOMContentLoaded", function () {
    bindElements();
    bindEvents();
    loadTopologyFromDefault();
  });

  function bindElements() {
    els.workspace = document.querySelector(".workspace");
    els.inspector = document.querySelector(".inspector");
    els.inspectorResizer = document.getElementById("inspectorResizer");
    els.canvasWrap = document.getElementById("canvasWrap");
    els.linkLayer = document.getElementById("linkLayer");
    els.nodeLayer = document.getElementById("nodeLayer");
    els.logBody = document.getElementById("logBody");
    els.inspectorIcon = document.getElementById("inspectorIcon");
    els.inspectorTitle = document.getElementById("inspectorTitle");
    els.inspectorSubtitle = document.getElementById("inspectorSubtitle");
    els.inspectorBody = document.getElementById("inspectorBody");
    els.searchInput = document.getElementById("searchInput");
    els.commandInput = document.getElementById("commandInput");
    els.sendCommandBtn = document.getElementById("sendCommandBtn");
    els.commandPreset = document.getElementById("commandPreset");
    els.fileInput = document.getElementById("fileInput");
    els.sourceBadge = document.getElementById("sourceBadge");
    els.engineStatus = document.getElementById("engineStatus");
    els.packetCounter = document.getElementById("packetCounter");
    els.topologyFile = document.getElementById("topologyFile");
    els.selectedTool = document.getElementById("selectedTool");
    els.closeInspectorBtn = document.getElementById("closeInspectorBtn");
    els.refreshBackendBtn = document.getElementById("refreshBackendBtn");
    els.modeStatus = document.getElementById("modeStatus");
    els.canvasHint = document.getElementById("canvasHint");
    els.activePalette = document.getElementById("activePalette");
    els.exportSvgBtn = document.getElementById("exportSvgBtn");
    els.reloadTopoBtn = document.getElementById("reloadTopoBtn");
    els.canvasBanner = document.getElementById("canvasBanner");
    els.configShortcutBtn = document.getElementById("configShortcutBtn");
    els.inspectShortcutBtn = document.getElementById("inspectShortcutBtn");
    els.linkConfigPanel = document.getElementById("linkConfigPanel");
    els.linkDelayInput = document.getElementById("linkDelayInput");
    els.linkMtuInput = document.getElementById("linkMtuInput");
    els.navFileBtn = document.getElementById("navFileBtn");
    els.navEditBtn = document.getElementById("navEditBtn");
    els.navSimulationBtn = document.getElementById("navSimulationBtn");
    els.navViewBtn = document.getElementById("navViewBtn");
    els.navToolsBtn = document.getElementById("navToolsBtn");
    els.clearCanvasBtn = document.getElementById("clearCanvasBtn");
    els.deleteNodeBtn = document.getElementById("deleteNodeBtn");
  }

  function bindEvents() {
    document.getElementById("loadTopologyBtn").addEventListener("click", function () {
      els.fileInput.click();
    });

    els.fileInput.addEventListener("change", function (event) {
      var file = event.target.files && event.target.files[0];
      if (!file) return;
      var reader = new FileReader();
      reader.onload = function () {
        if (state.apiAvailable) {
          importTopologyFile(reader.result, file.name);
          return;
        }
        try {
          setTopology(JSON.parse(reader.result), file.name);
          addLog("System", "system", "Loaded topology from " + file.name);
        } catch (error) {
          addLog("System", "system", "Invalid topology JSON: " + error.message);
        }
      };
      reader.readAsText(file);
    });

    els.sendCommandBtn.addEventListener("click", runCommand);
    els.commandInput.addEventListener("keydown", function (event) {
      if (event.key === "Enter" && !event.shiftKey) {
        event.preventDefault();
        runCommand();
      }
    });

    document.getElementById("resetBtn").addEventListener("click", function () {
      if (state.apiAvailable) {
        executeCommand("load topology.json");
      } else {
        setTopology(state.topology, state.source);
        addLog("System", "system", "Dashboard view reset");
      }
    });

    document.getElementById("fitBtn").addEventListener("click", function () {
      state.zoom = 1;
      state.manualPositions = {};
      renderTopology();
    });

    document.getElementById("zoomInBtn").addEventListener("click", function () {
      state.zoom = Math.min(1.4, state.zoom + 0.1);
      renderTopology();
    });

    document.getElementById("zoomOutBtn").addEventListener("click", function () {
      state.zoom = Math.max(0.75, state.zoom - 0.1);
      renderTopology();
    });

    document.getElementById("clearLogBtn").addEventListener("click", function () {
      els.logBody.innerHTML = "";
      addLog("System", "system", "Event log cleared");
    });

    document.getElementById("saveSnapshotBtn").addEventListener("click", downloadSnapshot);

    if (els.commandPreset) {
      els.commandPreset.addEventListener("change", function () {
        if (els.commandPreset.value) {
          els.commandInput.value = els.commandPreset.value;
          els.commandInput.focus();
        }
      });
    }

    document.querySelectorAll("[data-command]").forEach(function (button) {
      button.addEventListener("click", function () {
        els.commandInput.value = button.dataset.command || "";
        runCommand();
      });
    });

    if (els.refreshBackendBtn) {
      els.refreshBackendBtn.addEventListener("click", function () {
        refreshTopology();
      });
    }

    if (els.exportSvgBtn) {
      els.exportSvgBtn.addEventListener("click", function () {
        executeCommand("visualize topology.svg").then(function (result) {
          if (result && result.ok) {
            addLog("System", "system", "SVG topology available at /topology.svg");
            window.open("/topology.svg", "_blank", "noopener");
          }
        });
      });
    }

    if (els.reloadTopoBtn) {
      els.reloadTopoBtn.addEventListener("click", function () {
        if (state.apiAvailable) {
          executeCommand("load topology.json");
        } else {
          loadStaticTopology();
        }
      });
    }

    if (els.linkDelayInput) {
      els.linkDelayInput.addEventListener("change", function () {
        state.linkDraft.delay = clampNumber(els.linkDelayInput.value, 0, 10000, 5);
        els.linkDelayInput.value = state.linkDraft.delay;
        updateCanvasHint();
      });
    }

    if (els.linkMtuInput) {
      els.linkMtuInput.addEventListener("change", function () {
        state.linkDraft.mtu = clampNumber(els.linkMtuInput.value, 576, 9000, 1500);
        els.linkMtuInput.value = state.linkDraft.mtu;
        updateCanvasHint();
      });
    }

    if (els.navFileBtn) {
      els.navFileBtn.addEventListener("click", function () {
        els.fileInput.click();
      });
    }

    if (els.navEditBtn) {
      els.navEditBtn.addEventListener("click", function () {
        setMode("deploy");
      });
    }

    if (els.navSimulationBtn) {
      els.navSimulationBtn.addEventListener("click", function () {
        els.commandInput.focus();
        els.commandInput.select();
      });
    }

    if (els.navViewBtn) {
      els.navViewBtn.addEventListener("click", function () {
        state.zoom = 1;
        state.manualPositions = {};
        renderTopology();
        addLog("System", "system", "Topology view centered");
      });
    }

    if (els.navToolsBtn) {
      els.navToolsBtn.addEventListener("click", function () {
        state.selectedTab = state.selectedNode && state.selectedNode.type === "host" ? "services" : "config";
        syncTabs();
        renderInspector();
      });
    }

    els.searchInput.addEventListener("input", renderTopology);

    document.querySelectorAll(".tab").forEach(function (tab) {
      tab.addEventListener("click", function () {
        state.selectedTab = tab.dataset.tab;
        document.querySelectorAll(".tab").forEach(function (item) {
          item.classList.toggle("active", item === tab);
        });
        renderInspector();
      });
    });

    document.querySelectorAll(".mode-btn").forEach(function (button) {
      button.addEventListener("click", function () {
        setMode(button.dataset.mode || "select");
      });
    });

    document.querySelectorAll(".rail-item[data-tool]").forEach(function (item) {
      item.addEventListener("click", function () {
        document.querySelectorAll(".rail-item[data-tool]").forEach(function (rail) {
          rail.classList.toggle("active", rail === item);
        });
        state.selectedToolType = item.dataset.tool || "switch";
        setMode("deploy");
        updateCanvasHint();
      });
    });

    document.getElementById("addNodeBtn").addEventListener("click", function () {
      createNodeFromTool(centerCanvasPosition());
    });

    if (els.clearCanvasBtn) {
      els.clearCanvasBtn.addEventListener("click", function () {
        clearTopologyCanvas();
      });
    }

    if (els.deleteNodeBtn) {
      els.deleteNodeBtn.addEventListener("click", function () {
        deleteSelectedNode();
      });
    }

    if (els.closeInspectorBtn) {
      els.closeInspectorBtn.addEventListener("click", function () {
        els.workspace.classList.remove("inspector-open");
      });
    }

    if (els.inspectorResizer) {
      els.inspectorResizer.addEventListener("pointerdown", startInspectorResize);
    }

    if (els.configShortcutBtn) {
      els.configShortcutBtn.addEventListener("click", function () {
        state.selectedTab = "config";
        syncTabs();
        renderInspector();
      });
    }

    if (els.inspectShortcutBtn) {
      els.inspectShortcutBtn.addEventListener("click", function () {
        state.selectedTab = "inspect";
        syncTabs();
        renderInspector();
        runDefaultInspectCommand();
      });
    }

    els.canvasWrap.addEventListener("click", handleCanvasClick);
    els.inspectorBody.addEventListener("click", handleInspectorClick);
    els.inspectorBody.addEventListener("submit", handleInspectorSubmit);

    window.addEventListener("resize", function () {
      if (window.innerWidth > 860) els.workspace.classList.remove("inspector-open");
      syncInspectorWidth();
      renderTopology();
    });

    syncInspectorWidth();
    updateCanvasHint();
  }

  function startInspectorResize(event) {
    if (!els.inspector || !els.inspectorResizer || window.innerWidth <= 860) {
      return;
    }

    event.preventDefault();
    state.inspectorResize = {
      startX: event.clientX,
      startWidth: currentInspectorWidth()
    };
    document.body.classList.add("inspector-resizing");
    els.inspector.classList.add("is-resizing");
    if (els.inspectorResizer.setPointerCapture) {
      els.inspectorResizer.setPointerCapture(event.pointerId);
    }
    window.addEventListener("pointermove", handleInspectorResize);
    window.addEventListener("pointerup", stopInspectorResize, { once: true });
  }

  function handleInspectorResize(event) {
    if (!state.inspectorResize) {
      return;
    }

    var delta = state.inspectorResize.startX - event.clientX;
    state.inspectorWidth = clampNumber(
      state.inspectorResize.startWidth + delta,
      320,
      maxInspectorWidth()
    );
    syncInspectorWidth();
    renderTopology();
  }

  function stopInspectorResize(event) {
    window.removeEventListener("pointermove", handleInspectorResize);
    if (els.inspector) {
      els.inspector.classList.remove("is-resizing");
    }
    document.body.classList.remove("inspector-resizing");
    if (els.inspectorResizer && els.inspectorResizer.releasePointerCapture && event) {
      try {
        els.inspectorResizer.releasePointerCapture(event.pointerId);
      } catch (error) {
        // Ignore release failures when the pointer lifecycle already ended.
      }
    }
    state.inspectorResize = null;
  }

  function currentInspectorWidth() {
    if (state.inspectorWidth) {
      return state.inspectorWidth;
    }
    return els.inspector ? Math.round(els.inspector.getBoundingClientRect().width) || 338 : 338;
  }

  function maxInspectorWidth() {
    var workspaceWidth = els.workspace ? els.workspace.clientWidth : window.innerWidth;
    return Math.max(320, Math.min(640, workspaceWidth - 260));
  }

  function syncInspectorWidth() {
    if (!els.inspector) {
      return;
    }
    if (window.innerWidth <= 860) {
      els.inspector.style.removeProperty("--inspector-width");
      return;
    }
    state.inspectorWidth = clampNumber(currentInspectorWidth(), 320, maxInspectorWidth());
    els.inspector.style.setProperty("--inspector-width", state.inspectorWidth + "px");
  }

  function readCanvasSize() {
    return {
      width: Math.max(280, els.canvasWrap.clientWidth || 900),
      height: Math.max(260, els.canvasWrap.clientHeight || 520)
    };
  }

  function syncCanvasGeometry() {
    var nextSize = readCanvasSize();
    var previousSize = state.canvasSize;

    if (!previousSize || !previousSize.width || !previousSize.height) {
      state.canvasSize = nextSize;
      return nextSize;
    }

    if (previousSize.width === nextSize.width && previousSize.height === nextSize.height) {
      return nextSize;
    }

    var previousCenterX = previousSize.width / 2;
    var previousCenterY = previousSize.height / 2;
    var nextCenterX = nextSize.width / 2;
    var nextCenterY = nextSize.height / 2;
    var scaleX = nextSize.width / Math.max(1, previousSize.width);
    var scaleY = nextSize.height / Math.max(1, previousSize.height);

    Object.keys(state.manualPositions || {}).forEach(function (nodeId) {
      var pos = state.manualPositions[nodeId];
      if (!pos) {
        return;
      }
      state.manualPositions[nodeId] = clampPosition({
        x: nextCenterX + ((pos.x - previousCenterX) * scaleX),
        y: nextCenterY + ((pos.y - previousCenterY) * scaleY)
      });
    });

    state.canvasSize = nextSize;
    return nextSize;
  }

  function loadTopologyFromDefault() {
    fetch("/api/health", { cache: "no-store" })
      .then(function (response) {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.json();
      })
      .then(function () {
        state.apiAvailable = true;
        setEngineStatus("Simulation: Connected to C++ backend");
        if (els.sourceBadge) {
          els.sourceBadge.textContent = "live backend";
        }
        if (els.canvasBanner) {
          els.canvasBanner.textContent = "Live backend connected. Select a node or deploy a new device.";
        }
        return refreshTopology();
      })
      .catch(function () {
        state.apiAvailable = false;
        if (els.canvasBanner) {
          els.canvasBanner.textContent = "Offline preview mode. Start magi_web for full simulator control.";
        }
        loadStaticTopology();
      });
  }

  function loadStaticTopology() {
    fetch("../topology.json", { cache: "no-store" })
      .then(function (response) {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.json();
      })
      .then(function (json) {
        setTopology(json, "topology.json");
        addLog("System", "system", "Loaded ../topology.json");
      })
      .catch(function () {
        setTopology(fallbackTopology, "fallback topology");
        addLog("System", "system", "Using embedded fallback topology. Use upload to load topology.json when opened from file.");
      });
  }

  function refreshTopology() {
    if (!state.apiAvailable) {
      loadStaticTopology();
      return Promise.resolve();
    }

    return fetch("/api/topology", { cache: "no-store" })
      .then(function (response) {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.json();
      })
      .then(function (json) {
        setTopology(json, "live backend");
        addLog("Backend", "system", "Topology refreshed from simulator");
      })
      .catch(function (error) {
        state.apiAvailable = false;
        setEngineStatus("Simulation: Backend disconnected");
        addLog("Backend", "error", "Backend refresh failed: " + error.message);
        if (els.canvasBanner) {
          els.canvasBanner.textContent = "Backend disconnected. Falling back to static topology.";
        }
        loadStaticTopology();
      });
  }

  function importTopologyFile(fileText, fileName, options) {
    options = options || {};
    setEngineStatus("Simulation: Importing topology");
    return fetch("/api/import-topology", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: fileText
    })
      .then(function (response) {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.json();
      })
      .then(function (result) {
        state.lastCommand = options.commandLabel || ("import " + fileName);
        state.lastCommandOutput = result.output || options.successMessage || "Topology imported.";
        if (result.topology) {
          setTopology(result.topology, fileName);
        }
        renderCommandOutput(result.output || options.successMessage || ("Imported " + fileName), result.ok);
        setEngineStatus(result.ok ? "Simulation: Ready" : "Simulation: Import failed");
        renderInspector();
        return result;
      })
      .catch(function (error) {
        addLog("Backend", "error", "Import failed: " + error.message);
        setEngineStatus("Simulation: Import failed");
        return { ok: false, output: error.message };
      });
  }

  function setTopology(topology, source) {
    var selectedId = state.selectedNode && state.selectedNode.id;
    var previousManualPositions = state.manualPositions || {};
    state.topology = topology || fallbackTopology;
    state.source = source || "topology.json";
    state.nodes = normalizeNodes(state.topology);
    state.links = normalizeLinks(state.topology);
    state.positions = {};
    state.manualPositions = {};
    state.nodes.forEach(function (node) {
      if (previousManualPositions[node.id]) {
        state.manualPositions[node.id] = previousManualPositions[node.id];
      }
      if (state.pendingPlacements[node.id]) {
        state.manualPositions[node.id] = state.pendingPlacements[node.id];
        delete state.pendingPlacements[node.id];
      }
    });
    state.selectedNode = state.nodes.find(function (node) { return node.id === selectedId; }) || state.nodes[0] || null;
    if (els.sourceBadge) {
      els.sourceBadge.textContent = state.source;
    }
    els.topologyFile.textContent = "Topology: " + state.source;
    computePositions();
    renderTopology();
    renderInspector();
    updateCounters();
    updateCanvasHint();
  }

  function normalizeNodes(topology) {
    var nodes = [];
    (topology.hosts || []).forEach(function (host) {
      nodes.push({
        id: host.name,
        name: host.name,
        type: "host",
        ip: host.ip_address || "",
        gateway: host.default_gateway || "",
        raw: host
      });
    });
    (topology.switches || []).forEach(function (sw) {
      nodes.push({
        id: sw.name,
        name: sw.name,
        type: "switch",
        ports: sw.num_ports || 24,
        vlans: sw.vlans || [],
        raw: sw
      });
    });
    (topology.routers || []).forEach(function (router) {
      nodes.push({
        id: router.name,
        name: router.name,
        type: "router",
        ports: router.num_ports || 4,
        interfaces: router.interfaces || [],
        routes: router.routing_table || [],
        raw: router
      });
    });
    return nodes;
  }

  function normalizeLinks(topology) {
    return (topology.links || []).map(function (link, index) {
      var a = parseEndpoint((link.endpoints || [])[0] || "");
      var b = parseEndpoint((link.endpoints || [])[1] || "");
      return {
        id: "link-" + index,
        a: a.node,
        aPort: a.port,
        b: b.node,
        bPort: b.port,
        delay: link.delay || 0,
        mtu: link.mtu || 1500
      };
    }).filter(function (link) {
      return link.a && link.b;
    });
  }

  function parseEndpoint(endpoint) {
    var parts = String(endpoint || "").split(":");
    return {
      node: parts[0],
      port: parts[1] || "1"
    };
  }

  function computePositions() {
    var canvasSize = syncCanvasGeometry();
    var width = canvasSize.width;
    var height = canvasSize.height;
    var cx = width / 2;
    var cy = height / 2;
    var paddingX = width < 520 ? 98 : 150;
    var paddingY = height < 420 ? 112 : 150;
    var maxRadius = Math.min(Math.max(64, (width - paddingX) / 2), Math.max(58, (height - paddingY) / 2));
    var radius = Math.max(58, maxRadius * state.zoom);
    var count = state.nodes.length;

    state.nodes.forEach(function (node, index) {
      if (state.manualPositions[node.id]) {
        state.positions[node.id] = clampPosition(state.manualPositions[node.id]);
        return;
      }
      var angle = count <= 1 ? -Math.PI / 2 : (Math.PI * 2 * index / count) - Math.PI / 2;
      state.positions[node.id] = {
        x: cx + Math.cos(angle) * radius,
        y: cy + Math.sin(angle) * radius
      };
    });
  }

  function renderTopology() {
    computePositions();
    var filter = (els.searchInput.value || "").trim().toLowerCase();
    renderLinks();
    els.nodeLayer.innerHTML = "";

    state.nodes.forEach(function (node) {
      var pos = state.positions[node.id];
      var visible = !filter || node.name.toLowerCase().indexOf(filter) !== -1 || node.type.indexOf(filter) !== -1;
      var kind = deviceKind(node);
      var el = document.createElement("button");
      el.className = "node" +
        (state.selectedNode && state.selectedNode.id === node.id ? " selected" : "") +
        (state.pendingLinkSource && state.pendingLinkSource.id === node.id ? " link-source" : "");
      el.style.left = pos.x + "px";
      el.style.top = pos.y + "px";
      el.style.setProperty("--node-opacity", visible ? "1" : "0.28");
      el.style.animationDelay = Math.min(220, state.nodes.indexOf(node) * 42) + "ms";
      el.setAttribute("type", "button");
      el.setAttribute("aria-label", "Inspect " + node.name);
      el.innerHTML =
        '<div class="node-card device-' + escapeHtml(kind) + '">' +
        deviceArtwork(node) +
        '<i class="node-status"></i>' +
        "</div>" +
        '<div class="node-label">' + escapeHtml(node.name) + "</div>";
      attachNodeInteractions(el, node);
      els.nodeLayer.appendChild(el);
    });
  }

  function renderLinks() {
    var width = Math.max(280, els.canvasWrap.clientWidth || 900);
    var height = Math.max(260, els.canvasWrap.clientHeight || 520);
    els.linkLayer.setAttribute("viewBox", "0 0 " + width + " " + height);
    els.linkLayer.innerHTML = "";

    state.links.forEach(function (link) {
      var a = state.positions[link.a];
      var b = state.positions[link.b];
      if (!a || !b) return;

      var line = svg("line", {
        x1: a.x,
        y1: a.y,
        x2: b.x,
        y2: b.y,
        class: "link-line" + (isSelectedLink(link) ? " selected-link" : "")
      });
      els.linkLayer.appendChild(line);

      var label = svg("text", {
        x: (a.x + b.x) / 2,
        y: (a.y + b.y) / 2 - 9,
        "text-anchor": "middle",
        class: "link-label"
      });
      label.textContent = link.a + ":" + link.aPort + " - " + link.b + ":" + link.bPort + " | " + link.delay + " ms / MTU " + link.mtu;
      els.linkLayer.appendChild(label);
    });

    if (state.pendingLinkSource && state.selectedNode && state.pendingLinkSource.id !== state.selectedNode.id) {
      var sourcePos = state.positions[state.pendingLinkSource.id];
      var selectedPos = state.positions[state.selectedNode.id];
      if (sourcePos && selectedPos) {
        els.linkLayer.appendChild(svg("line", {
          x1: sourcePos.x,
          y1: sourcePos.y,
          x2: selectedPos.x,
          y2: selectedPos.y,
          class: "link-line pending-link"
        }));
      }
    }
  }

  function attachNodeInteractions(el, node) {
    el.addEventListener("pointerdown", function (event) {
      if (event.button !== 0) return;
      if (state.mode !== "select") return;
      var pos = state.positions[node.id];
      state.drag = {
        id: node.id,
        pointerId: event.pointerId,
        startX: event.clientX,
        startY: event.clientY,
        originX: pos.x,
        originY: pos.y,
        moved: false,
        element: el
      };
      el.setPointerCapture(event.pointerId);
      el.classList.add("dragging");
    });

    el.addEventListener("pointermove", function (event) {
      var drag = state.drag;
      if (!drag || drag.id !== node.id || drag.pointerId !== event.pointerId) return;
      var dx = event.clientX - drag.startX;
      var dy = event.clientY - drag.startY;
      if (Math.abs(dx) + Math.abs(dy) > 4) drag.moved = true;
      if (!drag.moved) return;
      var next = clampPosition({ x: drag.originX + dx, y: drag.originY + dy });
      state.manualPositions[node.id] = next;
      state.positions[node.id] = next;
      el.style.left = next.x + "px";
      el.style.top = next.y + "px";
      renderLinks();
    });

    el.addEventListener("pointerup", function (event) {
      var drag = state.drag;
      if (drag && drag.id === node.id && drag.pointerId === event.pointerId) {
        el.classList.remove("dragging");
        state.drag = null;
        if (!drag.moved) handleNodeActivation(node);
        return;
      }
      if (state.mode !== "select") {
        handleNodeActivation(node);
      }
    });

    el.addEventListener("pointercancel", function () {
      el.classList.remove("dragging");
      state.drag = null;
    });

    el.addEventListener("keydown", function (event) {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        handleNodeActivation(node);
      }
    });
  }

  function handleNodeActivation(node) {
    if (state.mode === "link") {
      handleLinkSelection(node);
      return;
    }
    selectNode(node);
  }

  function selectNode(node) {
    state.selectedNode = node;
    if (window.innerWidth <= 860) els.workspace.classList.add("inspector-open");
    renderTopology();
    renderInspector();
    updateCanvasHint();
  }

  function clampPosition(pos) {
    var width = Math.max(280, els.canvasWrap.clientWidth || 900);
    var height = Math.max(260, els.canvasWrap.clientHeight || 520);
    var margin = window.innerWidth < 680 ? 48 : 68;
    return {
      x: Math.min(width - margin, Math.max(margin, pos.x)),
      y: Math.min(height - margin, Math.max(margin, pos.y))
    };
  }

  function isSelectedLink(link) {
    return state.selectedNode && (link.a === state.selectedNode.id || link.b === state.selectedNode.id);
  }

  function renderInspector() {
    var node = state.selectedNode;
    if (!node) {
      els.inspectorTitle.textContent = "No Node";
      els.inspectorSubtitle.textContent = "topology is empty";
      els.inspectorBody.innerHTML = '<div class="empty-state">Load a topology to inspect devices.</div>';
      return;
    }

    els.inspectorIcon.innerHTML = '<span class="material-symbols-outlined">' + iconFor(node.type, node.name) + "</span>";
    els.inspectorTitle.textContent = node.name;
    els.inspectorSubtitle.textContent = node.type + " | active";
    syncTabs();

    var body = overviewHtml(node);
    if (state.selectedTab === "config") {
      body = configHtml(node);
    } else if (state.selectedTab === "inspect") {
      body = inspectHtml(node);
    } else if (state.selectedTab === "services") {
      body = servicesHtml(node);
    }
    els.inspectorBody.innerHTML = body + consoleHtml();
  }

  function syncTabs() {
    document.querySelectorAll(".tab").forEach(function (item) {
      item.classList.toggle("active", item.dataset.tab === state.selectedTab);
    });
  }

  function overviewHtml(node) {
    var links = linksForNode(node.id);
    return [
      '<section class="info-block">',
      '<div class="summary-grid">',
      '<div class="summary-card"><span>Address</span><strong>' + escapeHtml(primaryAddress(node)) + '</strong></div>',
      '<div class="summary-card"><span>Links</span><strong>' + links.length + '</strong></div>',
      '<div class="summary-card"><span>Ports</span><strong>' + portCountForNode(node) + '</strong></div>',
      '</div>',
      '</section>',
      '<section class="info-block">',
      '<h3>Connected Links</h3>',
      links.length ? linkTable(node, links) : '<div class="empty-state">No active links for this node.</div>',
      '</section>'
    ].join("");
  }

  function quickActionButtonForOverview(node, targetIp) {
    var buttons = [];
    if (node.type === "host") {
      if (targetIp) {
        buttons.push(commandButton(node.name + " ping " + targetIp, "Ping Peer", "mini-button"));
        buttons.push(commandButton(node.name + " traceroute " + targetIp, "Traceroute", "soft-button"));
      }
      buttons.push(commandButton(node.name + " arp", "ARP Cache", "soft-button"));
      buttons.push(tabButton("config", "Configure IP", "mini-button"));
    } else if (node.type === "switch") {
      buttons.push(commandButton(node.name + " mac", "MAC Table", "mini-button"));
      buttons.push(tabButton("config", "Edit VLAN", "soft-button"));
      buttons.push(commandButton("topology", "Show Topology", "soft-button"));
    } else {
      buttons.push(commandButton(node.name + " route", "Routing Table", "mini-button"));
      buttons.push(commandButton(node.name + " arp", "ARP Cache", "soft-button"));
      buttons.push(commandButton("rip show " + node.name, "RIP Routes", "soft-button"));
      buttons.push(tabButton("services", "ACL / NAT / RIP", "mini-button"));
    }
    return buttons.join("");
  }

  function configHtml(node) {
    if (node.type === "host") return hostConfigHtml(node);
    if (node.type === "switch") return switchConfigHtml(node);
    return routerConfigHtml(node);
  }

  function inspectHtml(node) {
    if (node.type === "host") {
      return [
        '<section class="info-block"><h3>Interface</h3>',
        table(["Interface", "IP Address", "Gateway"], [["eth0", node.ip || "not set", node.gateway || "not set"]]),
        '</section>',
        '<section class="info-block"><div class="section-card"><h4>Diagnostics</h4><div class="action-grid">',
        commandButton(node.name + " arp", "Show ARP", "mini-button"),
        commandButton("show " + node.name, "Show Node", "soft-button"),
        '</div></div></section>'
      ].join("");
    }
    if (node.type === "switch") {
      return [
        '<section class="info-block"><h3>Ports & VLAN</h3>',
        switchPortTable(node),
        '</section>',
        '<section class="info-block"><div class="section-card"><h4>Inspection</h4><div class="action-grid">',
        commandButton(node.name + " mac", "Show MAC Table", "mini-button"),
        commandButton("show " + node.name, "Show Node", "soft-button"),
        '</div></div></section>'
      ].join("");
    }
    return [
      '<section class="info-block"><h3>Interfaces</h3>',
      routerInterfaceTable(node),
      '</section>',
      '<section class="info-block"><h3>Routes</h3>',
      routerRouteTable(node),
      '</section>',
      '<section class="info-block"><div class="section-card"><h4>Inspection</h4><div class="action-grid">',
      commandButton(node.name + " route", "Show Routes", "mini-button"),
      commandButton(node.name + " arp", "Show ARP", "soft-button"),
      commandButton("rip show " + node.name, "Show RIP", "soft-button"),
      commandButton("show " + node.name, "Show Node", "mini-button"),
      '</div></div></section>'
    ].join("");
  }

  function servicesHtml(node) {
    if (node.type === "host") return hostServicesHtml(node);
    if (node.type === "switch") return switchServicesHtml(node);
    return routerServicesHtml(node);
  }

  function hostConfigHtml(node) {
    return [
      '<section class="info-block"><div class="section-card">',
      '<h4>Addressing</h4>',
      '<p>Configure host IP and default gateway, then refresh topology from the simulator.</p>',
      '<form class="stack-form" data-form="host-address" data-node="' + escapeHtml(node.name) + '">',
      '<div class="field-group"><label>IP address / CIDR</label><input name="ip_address" value="' + escapeHtml(node.ip || "") + '" placeholder="192.168.1.10/24" /></div>',
      '<div class="field-group"><label>Default gateway</label><input name="gateway" value="' + escapeHtml(node.gateway || "") + '" placeholder="192.168.1.1" /></div>',
      '<div class="button-row"><button class="mini-button" type="submit">Apply</button>' +
      commandButton(node.name + " dhcp_discover 3000 3", "DHCP Discover", "soft-button") + '</div>',
      '</form>',
      '</div></section>'
    ].join("");
  }

  function switchConfigHtml(node) {
    return [
      '<section class="info-block"><div class="section-card">',
      '<h4>Port VLAN Configuration</h4>',
      '<p>Set a switch port as access or trunk and push the CLI command automatically.</p>',
      '<form class="stack-form" data-form="switch-vlan" data-node="' + escapeHtml(node.name) + '">',
      '<div class="inline-triple">',
      '<div class="field-group"><label>Port</label><select name="port">' + portOptions(node.ports || 8, 1) + '</select></div>',
      '<div class="field-group"><label>Mode</label><select name="mode"><option value="access">access</option><option value="trunk">trunk</option></select></div>',
      '<div class="field-group"><label>VLAN / Native VLAN</label><input name="vlan_id" value="1" placeholder="1" /></div>',
      '</div>',
      '<div class="button-row"><button class="mini-button" type="submit">Apply VLAN</button>' +
      commandButton(node.name + " mac", "Show MAC Table", "soft-button") + '</div>',
      '</form>',
      '</div></section>',
      '<section class="info-block"><h3>Current Port Map</h3>',
      switchPortTable(node),
      '</section>'
    ].join("");
  }

  function routerConfigHtml(node) {
    return [
      '<section class="info-block"><div class="section-card">',
      '<h4>Interface Addressing</h4>',
      '<p>Use router port and optional VLAN sub-interface syntax just like the CLI.</p>',
      '<form class="stack-form" data-form="router-interface" data-node="' + escapeHtml(node.name) + '">',
      '<div class="inline-triple">',
      '<div class="field-group"><label>Port</label><select name="port">' + portOptions(node.ports || 4, firstRouterPort(node)) + '</select></div>',
      '<div class="field-group"><label>VLAN (optional)</label><input name="vlan_id" placeholder="blank or 10" /></div>',
      '<div class="field-group"><label>IP / CIDR</label><input name="cidr" value="' + escapeHtml(firstRouterCidr(node)) + '" placeholder="10.0.0.1/24" /></div>',
      '</div>',
      '<div class="button-row"><button class="mini-button" type="submit">Apply Interface</button>' +
      commandButton("rip show " + node.name, "Inspect RIP", "soft-button") + '</div>',
      '</form>',
      '</div></section>',
      '<section class="info-block"><div class="section-card">',
      '<h4>Static Route</h4>',
      '<p>Add route entries without typing the full CLI command.</p>',
      '<form class="stack-form" data-form="router-route" data-node="' + escapeHtml(node.name) + '">',
      '<div class="field-group"><label>Destination CIDR</label><input name="destination" placeholder="10.0.0.0/24" /></div>',
      '<div class="inline-pair">',
      '<div class="field-group"><label>Next hop IP</label><input name="next_hop" placeholder="192.168.1.2" /></div>',
      '<div class="field-group"><label>Out interface</label><input name="out_interface" placeholder="1 or 1.10" value="' + escapeHtml(firstRouterPort(node)) + '" /></div>',
      '</div>',
      '<div class="button-row"><button class="mini-button" type="submit">Add Route</button>' +
      commandButton(node.name + " route", "Show Routes", "soft-button") + '</div>',
      '</form>',
      '</div></section>'
    ].join("");
  }

  function hostServicesHtml(node) {
    var peerIp = defaultPeerIp(node);
    return [
      '<section class="info-block"><div class="section-card">',
      '<h4>Traffic Generator</h4>',
      '<p>Generate ICMP, TCP, UDP, or HTTP traffic from the selected host.</p>',
      '<form class="stack-form" data-form="host-traffic" data-node="' + escapeHtml(node.name) + '">',
      '<div class="field-group"><label>Action</label><select name="action">' +
      '<option value="ping">ping</option><option value="traceroute">traceroute</option>' +
      '<option value="tcp_connect">tcp_connect</option><option value="tcp_close">tcp_close</option>' +
      '<option value="udp_send">udp_send</option><option value="http_get">http_get</option></select></div>',
      '<div class="field-group"><label>Target</label><input name="target" value="' + escapeHtml(peerIp || "") + '" placeholder="192.168.1.11 or http://192.168.1.11/" /></div>',
      '<div class="inline-triple">',
      '<div class="field-group"><label>Port</label><input name="port" value="80" placeholder="80" /></div>',
      '<div class="field-group"><label>Source Port</label><input name="source_port" value="5000" placeholder="5000" /></div>',
      '<div class="field-group"><label>Destination Port</label><input name="destination_port" value="5001" placeholder="5001" /></div>',
      '</div>',
      '<div class="field-group"><label>Payload / File</label><input name="payload" value="MAGI_UDP_TEST" placeholder="payload or index.html" /></div>',
      '<div class="button-row"><button class="mini-button" type="submit">Send Traffic</button>' +
      commandButton("topology", "Refresh Links", "soft-button") + '</div>',
      '</form>',
      '</div></section>',
      '<section class="info-block"><div class="section-card">',
      '<h4>Application Services</h4>',
      '<p>Toggle HTTP, DHCP, or DNS services on this host or trigger DHCP discover.</p>',
      '<form class="stack-form" data-form="host-service" data-node="' + escapeHtml(node.name) + '">',
      '<div class="field-group"><label>Service Action</label><select name="service_action">' +
      '<option value="http_server_start">http_server start</option><option value="http_server_stop">http_server stop</option>' +
      '<option value="dhcp_server_start">dhcp_server start</option><option value="dhcp_server_stop">dhcp_server stop</option>' +
      '<option value="dhcp_discover">dhcp_discover</option><option value="dns_server_start">dns_server start</option>' +
      '<option value="dns_server_stop">dns_server stop</option></select></div>',
      '<div class="inline-triple">',
      '<div class="field-group"><label>File / Timeout</label><input name="service_arg1" value="index.html" placeholder="index.html or 3000" /></div>',
      '<div class="field-group"><label>Attempts / Extra</label><input name="service_arg2" value="3" placeholder="3" /></div>',
      '<div class="field-group"><label>Reserved</label><input name="service_arg3" placeholder="optional" /></div>',
      '</div>',
      '<div class="button-row"><button class="mini-button" type="submit">Run Service</button>' +
      commandButton(node.name + " arp", "Show ARP", "soft-button") + '</div>',
      '</form>',
      '</div></section>'
    ].join("");
  }

  function switchServicesHtml(node) {
    return [
      '<section class="info-block"><div class="section-card">',
      '<h4>Switch Inspection</h4>',
      '<p>Switches do not generate L7 traffic. Use these controls to inspect forwarding state.</p>',
      '<div class="action-grid">',
      commandButton(node.name + " mac", "Show MAC Table", "mini-button"),
      commandButton("show " + node.name, "Show Node", "soft-button"),
      tabButton("config", "Edit VLAN", "mini-button"),
      commandButton("topology", "Show Topology", "soft-button"),
      '</div>',
      '</div></section>'
    ].join("");
  }

  function routerServicesHtml(node) {
    return [
      '<section class="info-block"><div class="section-card">',
      '<h4>ACL Rules</h4>',
      '<p>Add, list, remove, or clear ACL rules on ingress or egress.</p>',
      '<form class="stack-form" data-form="router-acl" data-node="' + escapeHtml(node.name) + '">',
      '<div class="inline-triple">',
      '<div class="field-group"><label>Operation</label><select name="operation"><option value="list">list</option><option value="add">add</option><option value="remove">remove</option><option value="clear">clear</option></select></div>',
      '<div class="field-group"><label>Direction</label><select name="direction"><option value="ingress">ingress</option><option value="egress">egress</option></select></div>',
      '<div class="field-group"><label>Action / Rule ID</label><input name="action_or_id" value="permit" placeholder="permit or 1" /></div>',
      '</div>',
      '<div class="inline-pair">',
      '<div class="field-group"><label>Source CIDR</label><input name="source_cidr" value="192.168.1.0/24" placeholder="192.168.1.0/24" /></div>',
      '<div class="field-group"><label>Destination CIDR</label><input name="destination_cidr" value="10.0.0.0/8" placeholder="10.0.0.0/8" /></div>',
      '</div>',
      '<div class="inline-triple">',
      '<div class="field-group"><label>Protocol</label><select name="protocol"><option value="tcp">tcp</option><option value="udp">udp</option><option value="icmp">icmp</option><option value="any">any</option></select></div>',
      '<div class="field-group"><label>Source Port</label><input name="source_port" value="80" placeholder="80" /></div>',
      '<div class="field-group"><label>Destination Port</label><input name="destination_port" value="80" placeholder="80" /></div>',
      '</div>',
      '<div class="button-row"><button class="mini-button" type="submit">Run ACL Command</button>' +
      commandButton("show " + node.name, "Show Node", "soft-button") + '</div>',
      '</form>',
      '</div></section>',
      '<section class="info-block"><div class="section-card">',
      '<h4>NAT</h4>',
      '<p>Manage static or dynamic mappings and inside/outside interfaces.</p>',
      '<form class="stack-form" data-form="router-nat" data-node="' + escapeHtml(node.name) + '">',
      '<div class="inline-triple">',
      '<div class="field-group"><label>Operation</label><select name="operation"><option value="list">list</option><option value="static">static</option><option value="dynamic">dynamic</option><option value="inside">inside</option><option value="outside">outside</option><option value="remove">remove</option><option value="clear">clear</option></select></div>',
      '<div class="field-group"><label>Protocol</label><select name="protocol"><option value="tcp">tcp</option><option value="udp">udp</option></select></div>',
      '<div class="field-group"><label>Port / Interface</label><input name="port_or_interface" value="1" placeholder="1 or 80" /></div>',
      '</div>',
      '<div class="inline-pair">',
      '<div class="field-group"><label>Internal IP</label><input name="internal_ip" value="192.168.1.10" placeholder="192.168.1.10" /></div>',
      '<div class="field-group"><label>Internal Port</label><input name="internal_port" value="80" placeholder="80" /></div>',
      '</div>',
      '<div class="inline-pair">',
      '<div class="field-group"><label>External IP</label><input name="external_ip" value="203.0.113.1" placeholder="203.0.113.1" /></div>',
      '<div class="field-group"><label>External Port</label><input name="external_port" value="8080" placeholder="8080" /></div>',
      '</div>',
      '<div class="button-row"><button class="mini-button" type="submit">Run NAT Command</button>' +
      commandButton("nat " + node.name + " list", "Show NAT", "soft-button") + '</div>',
      '</form>',
      '</div></section>',
      '<section class="info-block"><div class="section-card">',
      '<h4>RIP</h4>',
      '<p>Enable, disable, update, show, or change auto-update interval.</p>',
      '<form class="stack-form" data-form="router-rip" data-node="' + escapeHtml(node.name) + '">',
      '<div class="inline-pair">',
      '<div class="field-group"><label>Operation</label><select name="operation"><option value="enable">enable</option><option value="disable">disable</option><option value="update">update</option><option value="show">show</option><option value="interval">interval</option></select></div>',
      '<div class="field-group"><label>Interval</label><input name="interval" value="3" placeholder="3" /></div>',
      '</div>',
      '<div class="button-row"><button class="mini-button" type="submit">Run RIP Command</button>' +
      commandButton("rip show " + node.name, "Show RIP", "soft-button") + '</div>',
      '</form>',
      '</div></section>'
    ].join("");
  }

  function consoleHtml() {
    var structuredTables = structuredConsoleTablesHtml(state.lastCommandOutput);
    return [
      '<section class="info-block">',
      '<div class="console-pane">',
      '<header><span>Last CLI output</span><span>' + escapeHtml(state.lastCommand || "No command") + '</span></header>',
      consoleOutputHtml(state.lastCommandOutput),
      '</div>',
      '</section>',
      structuredTables
    ].join("");
  }

  function consoleOutputHtml(output) {
    var lines = String(output || "").replace(/\r/g, "").split("\n");
    var hasVisibleLine = lines.some(function (line) {
      return line.trim().length > 0;
    });

    if (!hasVisibleLine) {
      lines = ["No output yet."];
    }

    return '<div class="console-output">' + lines.map(function (line) {
      if (!line.trim()) {
        return '<div class="console-line empty-console">&nbsp;</div>';
      }
      return '<div class="console-line">' + escapeHtml(line) + '</div>';
    }).join("") + '</div>';
  }

  function structuredConsoleTablesHtml(output) {
    var tables = parseStructuredConsoleTables(output);
    if (!tables.length) {
      return "";
    }

    return '<section class="info-block cli-table-stack">' + tables.map(function (tableDef) {
      var body = tableDef.headers && tableDef.headers.length && tableDef.rows && tableDef.rows.length
        ? '<div class="mini-table-wrap">' + table(tableDef.headers, tableDef.rows) + '</div>'
        : '<div class="empty-state">' + escapeHtml(tableDef.emptyMessage || "No rows available.") + '</div>';

      return [
        '<div class="section-card cli-table-card">',
        '<h4>' + escapeHtml(tableDef.title || "Structured output") + '</h4>',
        body,
        '</div>'
      ].join("");
    }).join("") + '</section>';
  }

  function parseStructuredConsoleTables(output) {
    var parsed = parseAsciiTableOutput(output) ||
      parseMacTableOutput(output) ||
      parseArpCacheOutput(output);

    return parsed ? [parsed] : [];
  }

  function parseAsciiTableOutput(output) {
    var lines = normalizedOutputLines(output);
    var titleIndex = lines.findIndex(function (line) {
      return /^\s*\[[^\]]+\]\s*$/.test(line);
    });

    if (titleIndex < 0) {
      return null;
    }

    var title = stripBracketTitle(lines[titleIndex]);
    var emptyLine = lines.slice(titleIndex + 1).find(function (line) {
      return line.trim() === "(empty)" || /tidak aktif/i.test(line);
    });

    var separatorIndex = lines.findIndex(function (line, index) {
      return index > titleIndex && /^\s*-{3,}(?:\s{2,}-+)+\s*$/.test(line);
    });

    if (separatorIndex < 0) {
      return emptyLine ? {
        title: title,
        headers: [],
        rows: [],
        emptyMessage: emptyLine.trim() === "(empty)" ? "No rows available." : emptyLine.trim()
      } : null;
    }

    var headerLine = lines[separatorIndex - 1] || "";
    var headers = splitAlignedColumns(headerLine);
    var rows = [];

    for (var i = separatorIndex + 1; i < lines.length; i += 1) {
      var line = lines[i];
      if (!line.trim()) {
        continue;
      }
      if (/^\s*\[/.test(line)) {
        break;
      }

      var cells = splitAlignedColumns(line);
      if (cells.length === headers.length) {
        rows.push(cells);
      }
    }

    return {
      title: title,
      headers: headers,
      rows: rows,
      emptyMessage: rows.length ? "" : "No rows available."
    };
  }

  function parseMacTableOutput(output) {
    var lines = normalizedOutputLines(output);
    var titleLine = lines.find(function (line) {
      return /^\s*\[MAC Table:/.test(line);
    });

    if (!titleLine) {
      return null;
    }

    var rows = [];
    lines.forEach(function (line) {
      var match = line.match(/^\s*(\d+):(.*)\s+->\s+port\s+(\d+)\s*$/i);
      if (!match) {
        return;
      }
      rows.push([match[1], match[2].trim(), match[3]]);
    });

    return {
      title: stripBracketTitle(titleLine),
      headers: ["VLAN", "MAC Address", "Port"],
      rows: rows,
      emptyMessage: "MAC table is empty."
    };
  }

  function parseArpCacheOutput(output) {
    var lines = normalizedOutputLines(output);
    var titleLine = lines.find(function (line) {
      return /^\s*\[ARP Cache:/.test(line);
    });

    if (!titleLine) {
      return null;
    }

    var rows = [];
    lines.forEach(function (line) {
      var match = line.match(/^\s*(\S+)\s+->\s+(\S+)\s*$/);
      if (!match) {
        return;
      }
      rows.push([match[1], match[2]]);
    });

    return {
      title: stripBracketTitle(titleLine),
      headers: ["IP Address", "MAC Address"],
      rows: rows,
      emptyMessage: "ARP cache is empty."
    };
  }

  function normalizedOutputLines(output) {
    return String(output || "").replace(/\r/g, "").split("\n").map(function (line) {
      return line.replace(/\s+$/, "");
    });
  }

  function stripBracketTitle(line) {
    return String(line || "").trim().replace(/^\[/, "").replace(/\]$/, "");
  }

  function splitAlignedColumns(line) {
    return String(line || "").trim().split(/\s{2,}/).filter(function (part) {
      return part.trim().length > 0;
    });
  }

  function linksForNode(nodeId) {
    return state.links.filter(function (link) {
      return link.a === nodeId || link.b === nodeId;
    });
  }

  function primaryAddress(node) {
    if (node.type === "router" && node.interfaces && node.interfaces.length) {
      return node.interfaces[0].ip_address || "unconfigured";
    }
    return node.ip || "unconfigured";
  }

  function portCountForNode(node) {
    if (node.type === "host") return 1;
    return node.ports || 1;
  }

  function defaultPeerIp(node) {
    var peerLink = state.links.find(function (link) {
      return link.a === node.id || link.b === node.id;
    });
    if (peerLink) {
      var peerId = peerLink.a === node.id ? peerLink.b : peerLink.a;
      var peerNode = state.nodes.find(function (item) { return item.id === peerId; });
      var peerAddress = primaryAddress(peerNode || {});
      if (peerAddress && peerAddress !== "unconfigured") {
        return stripCidr(peerAddress);
      }
    }
    var any = state.nodes.find(function (item) {
      return item.id !== node.id && primaryAddress(item) && primaryAddress(item) !== "unconfigured";
    });
    return any ? stripCidr(primaryAddress(any)) : "";
  }

  function firstRouterPort(node) {
    if (node.interfaces && node.interfaces.length) {
      return String(node.interfaces[0].port || node.interfaces[0].endpoint || "1");
    }
    return "1";
  }

  function firstRouterCidr(node) {
    if (node.interfaces && node.interfaces.length) {
      return node.interfaces[0].ip_address || "";
    }
    return "";
  }

  function routerInterfaceTable(node) {
    var interfaces = node.interfaces || [];
    if (!interfaces.length) {
      return '<div class="empty-state">No router interfaces configured.</div>';
    }
    return table(["Port", "IP Address", "State"], interfaces.map(function (item) {
      return [item.port || item.endpoint || "1", item.ip_address || "", htmlCell('<span class="status-dot"></span> up')];
    }));
  }

  function switchPortTable(node) {
    var rows = [];
    var ports = node.ports || 1;
    for (var i = 1; i <= ports; i += 1) {
      var vlan = (node.vlans || []).find(function (item) { return Number(item.port) === i; });
      rows.push([i, vlan ? vlan.mode : "access", vlan ? String(vlan.vlan_id || 1) : "1"]);
    }
    return table(["Port", "Mode", "VLAN"], rows);
  }

  function routerRouteTable(node) {
    var routes = node.routes || [];
    if (!routes.length) {
      return '<div class="empty-state">No static routes configured.</div>';
    }
    return table(["Destination", "Next Hop", "Interface"], routes.map(function (route) {
      return [route.destination || "", route.next_hop || "direct", route.interface || route.out_interface || ""];
    }));
  }

  function linkTable(node, links) {
    return table(["Peer", "Local Port", "Delay / MTU"], links.map(function (link) {
      var peer = link.a === node.id ? link.b : link.a;
      var port = link.a === node.id ? link.aPort : link.bPort;
      return [peer, port, link.delay + " ms / " + link.mtu];
    }));
  }

  function commandButton(command, label, className) {
    return '<button type="button" class="' + escapeHtml(className || "soft-button") + '" data-run-command="' + escapeHtml(command) + '">' + escapeHtml(label) + '</button>';
  }

  function tabButton(tab, label, className) {
    return '<button type="button" class="' + escapeHtml(className || "soft-button") + '" data-switch-tab="' + escapeHtml(tab) + '">' + escapeHtml(label) + '</button>';
  }

  function portOptions(count, selected) {
    var options = [];
    for (var i = 1; i <= count; i += 1) {
      options.push('<option value="' + i + '"' + (String(selected) === String(i) ? " selected" : "") + '>' + i + '</option>');
    }
    return options.join("");
  }

  function table(headers, rows) {
    return [
      '<table class="mini-table"><thead><tr>',
      headers.map(function (head) { return "<th>" + escapeHtml(head) + "</th>"; }).join(""),
      "</tr></thead><tbody>",
      rows.map(function (row) {
        return "<tr>" + row.map(function (cell) { return "<td>" + cellHtml(cell) + "</td>"; }).join("") + "</tr>";
      }).join(""),
      "</tbody></table>"
    ].join("");
  }

  function htmlCell(value) {
    return { html: value };
  }

  function cellHtml(value) {
    if (value && typeof value === "object" && Object.prototype.hasOwnProperty.call(value, "html")) {
      return value.html;
    }
    return escapeHtml(value);
  }

  function handleCanvasClick(event) {
    if (event.target.closest(".node")) return;

    if (state.mode === "deploy") {
      createNodeFromTool(clampPosition({ x: event.offsetX, y: event.offsetY }));
      return;
    }

    if (state.mode === "link") {
      state.pendingLinkSource = null;
      updateCanvasHint();
      renderTopology();
      return;
    }

    if (state.mode === "select") {
      state.selectedNode = null;
      renderTopology();
      renderInspector();
      updateCanvasHint();
    }
  }

  function setMode(mode) {
    state.mode = mode || "select";
    if (state.mode !== "link") {
      state.pendingLinkSource = null;
    }
    document.querySelectorAll(".mode-btn").forEach(function (button) {
      button.classList.toggle("active", button.dataset.mode === state.mode);
    });
    updateCanvasHint();
    renderTopology();
  }

  function updateCanvasHint() {
    els.canvasWrap.setAttribute("data-mode", state.mode);
    var paletteLabel = state.selectedToolType ? state.selectedToolType.charAt(0).toUpperCase() + state.selectedToolType.slice(1) : "Switch";
    syncSideRail();
    if (els.linkConfigPanel) {
      els.linkConfigPanel.classList.toggle("hidden", state.mode !== "link");
    }
    if (els.linkDelayInput) {
      els.linkDelayInput.value = state.linkDraft.delay;
    }
    if (els.linkMtuInput) {
      els.linkMtuInput.value = state.linkDraft.mtu;
    }
    if (els.modeStatus) {
      els.modeStatus.textContent = "Mode: " + state.mode.charAt(0).toUpperCase() + state.mode.slice(1);
    }
    if (els.activePalette) {
      els.activePalette.textContent = "Palette: " + paletteLabel;
    }
    if (!els.canvasHint) return;

    if (state.mode === "deploy") {
      els.canvasHint.textContent = "Deploy mode: click empty canvas to create a " + paletteLabel.toLowerCase() + " device.";
      return;
    }
    if (state.mode === "link") {
      if (state.pendingLinkSource) {
        els.canvasHint.textContent = "Link mode: select the second device to connect from " + state.pendingLinkSource.name + " (" + state.linkDraft.delay + " ms, MTU " + state.linkDraft.mtu + ").";
      } else {
        els.canvasHint.textContent = "Link mode: select the first device to start a connection (" + state.linkDraft.delay + " ms, MTU " + state.linkDraft.mtu + ").";
      }
      return;
    }
    if (!state.nodes.length) {
      els.canvasHint.textContent = "Topology empty. Choose a device and deploy a new node onto the canvas.";
      return;
    }
    els.canvasHint.textContent = state.selectedNode
      ? "Selected " + state.selectedNode.name + ". Drag to reposition or use the inspector to configure it."
      : "Select mode: click a node to inspect and configure it.";
  }

  function centerCanvasPosition() {
    var width = Math.max(280, els.canvasWrap.clientWidth || 900);
    var height = Math.max(260, els.canvasWrap.clientHeight || 520);
    return clampPosition({ x: width / 2, y: height / 2 });
  }

  function createNodeFromTool(position) {
    var descriptor = toolDescriptor(state.selectedToolType);
    var nextName = nextNameForTool(descriptor.prefix);
    var command = "create " + descriptor.type + " " + nextName + (descriptor.ports ? " " + descriptor.ports : "");

    if (!state.apiAvailable) {
      localCreateNode(descriptor, nextName, position || centerCanvasPosition());
      return;
    }

    state.pendingPlacements[nextName] = position || centerCanvasPosition();
    executeCommand(command).then(function (result) {
      if (result && result.ok) {
        var created = state.nodes.find(function (node) { return node.id === nextName; });
        if (created) {
          selectNode(created);
        }
      } else {
        delete state.pendingPlacements[nextName];
      }
    });
  }

  function localCreateNode(descriptor, nextName, position) {
    if (descriptor.type === "host") {
      state.topology.hosts = state.topology.hosts || [];
      state.topology.hosts.push({
        name: nextName,
        ip_address: "",
        default_gateway: ""
      });
    } else if (descriptor.type === "switch") {
      state.topology.switches = state.topology.switches || [];
      state.topology.switches.push({
        name: nextName,
        num_ports: descriptor.ports || 8,
        vlans: []
      });
    } else {
      state.topology.routers = state.topology.routers || [];
      state.topology.routers.push({
        name: nextName,
        num_ports: descriptor.ports || 4,
        interfaces: [],
        routing_table: []
      });
    }
    state.pendingPlacements[nextName] = position;
    setTopology(state.topology, "offline draft");
    var created = state.nodes.find(function (node) { return node.id === nextName; });
    if (created) {
      selectNode(created);
    }
    addLog("System", "system", "Offline draft created " + descriptor.type + " " + nextName);
  }

  function toolDescriptor(tool) {
    if (tool === "host") return { type: "host", prefix: "H", ports: "" };
    if (tool === "router") return { type: "router", prefix: "R", ports: 4 };
    if (tool === "server") return { type: "host", prefix: "SRV", ports: "" };
    if (tool === "firewall") return { type: "router", prefix: "FW", ports: 4 };
    return { type: "switch", prefix: "SW", ports: 8 };
  }

  function nextNameForTool(prefix) {
    var max = 0;
    state.nodes.forEach(function (node) {
      if (node.name.indexOf(prefix) !== 0) return;
      var suffix = parseInt(node.name.slice(prefix.length), 10);
      if (!isNaN(suffix)) {
        max = Math.max(max, suffix);
      }
    });
    return prefix + String(max + 1);
  }

  function handleLinkSelection(node) {
    if (!state.pendingLinkSource) {
      state.pendingLinkSource = node;
      selectNode(node);
      return;
    }

    if (state.pendingLinkSource.id === node.id) {
      state.pendingLinkSource = null;
      updateCanvasHint();
      renderTopology();
      return;
    }

    var sourcePort = firstAvailablePort(state.pendingLinkSource);
    var targetPort = firstAvailablePort(node);
    if (!sourcePort || !targetPort) {
      addLog("System", "error", "No free port available for one of the selected devices.");
      state.pendingLinkSource = null;
      updateCanvasHint();
      renderTopology();
      return;
    }

    var sourceEndpoint = endpointForNode(state.pendingLinkSource, sourcePort);
    var targetEndpoint = endpointForNode(node, targetPort);
    var linkCommand = "link " + sourceEndpoint + " " + targetEndpoint + " " + state.linkDraft.delay + " " + state.linkDraft.mtu;
    state.pendingLinkSource = null;
    executeCommand(linkCommand).then(function () {
      selectNode(node);
      setMode("select");
    });
  }

  function firstAvailablePort(node) {
    var maxPort = portCountForNode(node);
    for (var port = 1; port <= maxPort; port += 1) {
      var used = state.links.some(function (link) {
        return (link.a === node.id && String(link.aPort) === String(port)) ||
               (link.b === node.id && String(link.bPort) === String(port));
      });
      if (!used) return port;
    }
    return 0;
  }

  function deleteSelectedNode() {
    var node = state.selectedNode;
    if (!node) {
      addLog("System", "error", "Select a device before deleting.");
      return Promise.resolve({ ok: false, output: "No selected device" });
    }

    if (!window.confirm("Delete " + node.name + " and all connected links?")) {
      return Promise.resolve({ ok: false, output: "Delete cancelled" });
    }

    var nextTopology = topologyWithoutNode(state.topology, node.id);
    delete state.manualPositions[node.id];
    delete state.positions[node.id];
    delete state.pendingPlacements[node.id];
    if (state.pendingLinkSource && state.pendingLinkSource.id === node.id) {
      state.pendingLinkSource = null;
    }

    if (!state.apiAvailable) {
      state.lastCommand = "delete " + node.name;
      state.lastCommandOutput = "Deleted " + node.name + " in offline draft mode.";
      setTopology(nextTopology, state.source);
      addLog("System", "system", "Deleted " + node.name + " from offline topology.");
      renderInspector();
      return Promise.resolve({ ok: true, output: state.lastCommandOutput });
    }

    return importTopologyFile(JSON.stringify(nextTopology, null, 2), state.source, {
      commandLabel: "delete " + node.name,
      successMessage: "Deleted " + node.name + " and reloaded topology."
    });
  }

  function clearTopologyCanvas() {
    if (!state.nodes.length) {
      addLog("System", "error", "Topology is already empty.");
      return Promise.resolve({ ok: false, output: "Topology already empty" });
    }

    if (!window.confirm("Clear all devices and links from the topology canvas?")) {
      return Promise.resolve({ ok: false, output: "Clear cancelled" });
    }

    var nextTopology = emptyTopology();
    state.manualPositions = {};
    state.positions = {};
    state.pendingPlacements = {};
    state.pendingLinkSource = null;
    state.selectedNode = null;

    if (!state.apiAvailable) {
      state.lastCommand = "clear topology";
      state.lastCommandOutput = "Topology cleared in offline draft mode.";
      setTopology(nextTopology, "empty topology");
      addLog("System", "system", "Cleared all devices from offline topology.");
      renderInspector();
      return Promise.resolve({ ok: true, output: state.lastCommandOutput });
    }

    return importTopologyFile(JSON.stringify(nextTopology, null, 2), state.source, {
      commandLabel: "clear topology",
      successMessage: "Cleared all devices and reloaded topology."
    });
  }

  function topologyWithoutNode(topology, nodeId) {
    var nextTopology = JSON.parse(JSON.stringify(topology || fallbackTopology));
    nextTopology.hosts = (nextTopology.hosts || []).filter(function (host) {
      return host.name !== nodeId;
    });
    nextTopology.switches = (nextTopology.switches || []).filter(function (sw) {
      return sw.name !== nodeId;
    });
    nextTopology.routers = (nextTopology.routers || []).filter(function (router) {
      return router.name !== nodeId;
    });
    nextTopology.links = (nextTopology.links || []).filter(function (link) {
      return (link.endpoints || []).every(function (endpoint) {
        return parseEndpoint(endpoint).node !== nodeId;
      });
    });
    return nextTopology;
  }

  function emptyTopology() {
    return {
      hosts: [],
      switches: [],
      routers: [],
      links: []
    };
  }

  function syncSideRail() {
    var paletteLabel = state.selectedToolType ? state.selectedToolType.charAt(0).toUpperCase() + state.selectedToolType.slice(1) : "Switch";
    document.querySelectorAll(".rail-item[data-tool]").forEach(function (item) {
      item.classList.toggle("active", item.dataset.tool === state.selectedToolType);
    });
    if (els.deleteNodeBtn) {
      els.deleteNodeBtn.disabled = !state.selectedNode;
      els.deleteNodeBtn.title = state.selectedNode ? ("Delete " + state.selectedNode.name) : "Select a device to delete";
    }
    if (els.clearCanvasBtn) {
      els.clearCanvasBtn.disabled = !state.nodes.length;
      els.clearCanvasBtn.title = state.nodes.length ? "Clear all devices and links" : "Topology already empty";
    }
    if (!els.selectedTool) return;

    if (state.mode === "select") {
      els.selectedTool.textContent = "Tool: Select";
      return;
    }
    if (state.mode === "link") {
      els.selectedTool.textContent = "Tool: Link";
      return;
    }
    els.selectedTool.textContent = "Tool: " + paletteLabel;
  }

  function endpointForNode(node, port) {
    if (node.type === "host" && Number(port) === 1) {
      return node.name;
    }
    return node.name + ":" + port;
  }

  function handleInspectorClick(event) {
    var runButton = event.target.closest("[data-run-command]");
    if (runButton) {
      executeCommand(runButton.getAttribute("data-run-command"));
      return;
    }

    var tabButtonEl = event.target.closest("[data-switch-tab]");
    if (tabButtonEl) {
      state.selectedTab = tabButtonEl.getAttribute("data-switch-tab") || "overview";
      syncTabs();
      renderInspector();
    }
  }

  function handleInspectorSubmit(event) {
    event.preventDefault();
    var form = event.target;
    if (!form || !form.dataset || !form.dataset.form) return;

    if (form.dataset.form === "host-address") {
      submitHostAddress(form);
    } else if (form.dataset.form === "switch-vlan") {
      submitSwitchVlan(form);
    } else if (form.dataset.form === "router-interface") {
      submitRouterInterface(form);
    } else if (form.dataset.form === "router-route") {
      submitRouterRoute(form);
    } else if (form.dataset.form === "host-traffic") {
      submitHostTraffic(form);
    } else if (form.dataset.form === "host-service") {
      submitHostService(form);
    } else if (form.dataset.form === "router-acl") {
      submitRouterAcl(form);
    } else if (form.dataset.form === "router-nat") {
      submitRouterNat(form);
    } else if (form.dataset.form === "router-rip") {
      submitRouterRip(form);
    }
  }

  function submitHostAddress(form) {
    var node = form.dataset.node;
    var ip = formValue(form, "ip_address");
    var gateway = formValue(form, "gateway");
    var commands = [];
    if (ip) commands.push("setip " + node + " " + ip);
    if (gateway) commands.push("setgw " + node + " " + gateway);
    executeCommands(commands);
  }

  function submitSwitchVlan(form) {
    var node = form.dataset.node;
    var mode = formValue(form, "mode") || "access";
    var port = formValue(form, "port") || "1";
    var vlanId = formValue(form, "vlan_id") || "1";
    executeCommand("vlan " + mode + " " + node + " " + port + " " + vlanId);
  }

  function submitRouterInterface(form) {
    var node = form.dataset.node;
    var port = formValue(form, "port") || "1";
    var vlanId = formValue(form, "vlan_id");
    var cidr = formValue(form, "cidr");
    if (!cidr) return;
    var endpoint = node + ":" + port + (vlanId ? "." + vlanId : "");
    executeCommand("setip " + endpoint + " " + cidr);
  }

  function submitRouterRoute(form) {
    var node = form.dataset.node;
    var destination = formValue(form, "destination");
    var nextHop = formValue(form, "next_hop");
    var outInterface = formValue(form, "out_interface");
    if (!destination || !nextHop || !outInterface) return;
    executeCommand("route add " + node + " " + destination + " " + nextHop + " " + outInterface);
  }

  function submitHostTraffic(form) {
    var node = form.dataset.node;
    var action = formValue(form, "action");
    var target = formValue(form, "target");
    var port = formValue(form, "port") || "80";
    var sourcePort = formValue(form, "source_port") || "5000";
    var destinationPort = formValue(form, "destination_port") || "5001";
    var payload = formValue(form, "payload") || "MAGI_UDP_TEST";

    if (!action || !target) return;

    if (action === "ping" || action === "traceroute") {
      executeCommand(node + " " + action + " " + target);
      return;
    }
    if (action === "tcp_connect") {
      executeCommand(node + " tcp_connect " + target + " " + port);
      return;
    }
    if (action === "tcp_close") {
      executeCommand(node + " tcp_close " + target + " " + port);
      return;
    }
    if (action === "udp_send") {
      executeCommand(node + " udp_send " + target + " " + sourcePort + " " + destinationPort + " " + payload);
      return;
    }
    if (action === "http_get") {
      var httpTarget = target.indexOf("http://") === 0 || target.indexOf("https://") === 0 ? target : "http://" + target + "/";
      executeCommand(node + " http_get " + httpTarget);
    }
  }

  function submitHostService(form) {
    var node = form.dataset.node;
    var action = formValue(form, "service_action");
    var arg1 = formValue(form, "service_arg1");
    var arg2 = formValue(form, "service_arg2");
    if (!action) return;

    if (action === "http_server_start") {
      executeCommand(node + " http_server start " + (arg1 || "index.html"));
      return;
    }
    if (action === "http_server_stop") {
      executeCommand(node + " http_server stop");
      return;
    }
    if (action === "dhcp_server_start") {
      executeCommand(node + " dhcp_server start");
      return;
    }
    if (action === "dhcp_server_stop") {
      executeCommand(node + " dhcp_server stop");
      return;
    }
    if (action === "dhcp_discover") {
      executeCommand(node + " dhcp_discover " + (arg1 || "3000") + " " + (arg2 || "3"));
      return;
    }
    if (action === "dns_server_start") {
      executeCommand(node + " dns_server start");
      return;
    }
    if (action === "dns_server_stop") {
      executeCommand(node + " dns_server stop");
    }
  }

  function submitRouterAcl(form) {
    var node = form.dataset.node;
    var operation = formValue(form, "operation") || "list";
    var direction = formValue(form, "direction") || "ingress";
    var actionOrId = formValue(form, "action_or_id") || "permit";
    var sourceCidr = formValue(form, "source_cidr") || "0.0.0.0/0";
    var destinationCidr = formValue(form, "destination_cidr") || "0.0.0.0/0";
    var protocol = formValue(form, "protocol") || "tcp";
    var sourcePort = formValue(form, "source_port");
    var destinationPort = formValue(form, "destination_port");

    if (operation === "list") {
      executeCommand("acl " + node + " " + direction + " list");
      return;
    }
    if (operation === "clear") {
      executeCommand("acl " + node + " " + direction + " clear");
      return;
    }
    if (operation === "remove") {
      executeCommand("acl " + node + " " + direction + " remove " + actionOrId);
      return;
    }
    var command = "acl " + node + " " + direction + " add " + actionOrId + " " + sourceCidr + " " + destinationCidr;
    if (protocol && protocol !== "any") {
      command += " " + protocol;
      if (sourcePort) command += " " + sourcePort;
      if (destinationPort) command += " " + destinationPort;
    }
    executeCommand(command);
  }

  function submitRouterNat(form) {
    var node = form.dataset.node;
    var operation = formValue(form, "operation") || "list";
    var protocol = formValue(form, "protocol") || "tcp";
    var portOrInterface = formValue(form, "port_or_interface") || "1";
    var internalIp = formValue(form, "internal_ip");
    var internalPort = formValue(form, "internal_port");
    var externalIp = formValue(form, "external_ip");
    var externalPort = formValue(form, "external_port");

    if (operation === "list" || operation === "clear") {
      executeCommand("nat " + node + " " + operation);
      return;
    }
    if (operation === "inside" || operation === "outside") {
      executeCommand("nat " + node + " " + operation + " " + portOrInterface);
      return;
    }
    if (operation === "remove") {
      executeCommand("nat " + node + " remove " + internalIp + " " + internalPort + " " + protocol);
      return;
    }
    if (operation === "static") {
      executeCommand("nat " + node + " static " + internalIp + " " + internalPort + " " + externalIp + " " + externalPort + " " + protocol);
      return;
    }
    if (operation === "dynamic") {
      executeCommand("nat " + node + " dynamic " + internalIp + " " + internalPort + " " + externalIp + " " + protocol);
    }
  }

  function submitRouterRip(form) {
    var node = form.dataset.node;
    var operation = formValue(form, "operation") || "show";
    var interval = formValue(form, "interval") || "3";

    if (operation === "interval") {
      executeCommand("rip interval " + interval);
      return;
    }
    if (operation === "update" || operation === "show") {
      executeCommand("rip " + operation + " " + node);
      return;
    }
    executeCommand("rip " + operation + " " + node);
  }

  function executeCommands(commands) {
    commands = (commands || []).filter(Boolean);
    if (!commands.length) {
      return Promise.resolve({ ok: false, output: "" });
    }
    return commands.reduce(function (chain, command) {
      return chain.then(function () {
        return executeCommand(command);
      });
    }, Promise.resolve());
  }

  function formValue(form, name) {
    var field = form.elements[name];
    return field ? String(field.value || "").trim() : "";
  }

  function clampNumber(value, min, max, fallback) {
    var parsed = parseInt(value, 10);
    if (isNaN(parsed)) return fallback;
    return Math.min(max, Math.max(min, parsed));
  }

  function applyLogFilter() {
    Array.prototype.forEach.call(els.logBody.querySelectorAll("tr"), function (row) {
      var type = String(row.dataset.logType || "system").toLowerCase();
      var visible = state.logFilter === "all" || type === state.logFilter;
      row.classList.toggle("filtered-out", !visible);
    });
  }

  function clearActivePackets() {
    Array.prototype.forEach.call(document.querySelectorAll(".packet"), function (packet) {
      packet.remove();
    });
  }

  function runDefaultInspectCommand() {
    if (!state.selectedNode) return;
    if (state.selectedNode.type === "host") {
      executeCommand(state.selectedNode.name + " arp");
      return;
    }
    if (state.selectedNode.type === "switch") {
      executeCommand(state.selectedNode.name + " mac");
      return;
    }
    executeCommand(state.selectedNode.name + " route");
  }

  function stripCidr(value) {
    return String(value || "").split("/")[0];
  }

  function runCommand() {
    if (state.commandBatchRunning) {
      return;
    }

    var typedCommand = String(els.commandInput.value || "");
    var commands = normalizeCommandBatch(typedCommand);
    if (!commands.length) {
      els.commandInput.focus();
      return;
    }

    state.commandBatchRunning = true;
    setCommandComposerBusy(true, commands.length);
    els.commandInput.value = "";
    if (els.commandPreset) {
      els.commandPreset.value = "";
    }

    executeCommands(commands).then(function () {
      setCommandComposerBusy(false, 0);
      state.commandBatchRunning = false;
      els.commandInput.focus();
    });
  }

  function normalizeCommandBatch(rawValue) {
    return String(rawValue || "")
      .replace(/\r/g, "")
      .split("\n")
      .map(function (line) {
        return line.trim();
      })
      .filter(function (line) {
        return line.length > 0;
      });
  }

  function setCommandComposerBusy(isBusy, batchSize) {
    if (els.commandInput) {
      els.commandInput.disabled = isBusy;
    }
    if (els.commandPreset) {
      els.commandPreset.disabled = isBusy;
    }
    if (els.sendCommandBtn) {
      els.sendCommandBtn.disabled = isBusy;
      els.sendCommandBtn.textContent = isBusy ? (batchSize > 1 ? "Running " + batchSize : "Running") : "Send";
    }
  }

  function executeCommand(command) {
    command = String(command || "").trim();
    if (!command) return Promise.resolve({ ok: false, command: "", output: "" });

    if (!state.apiAvailable) {
      simulateCommand(command);
      state.lastCommand = command;
      state.lastCommandOutput = "Offline preview mode executed a visual-only simulation.";
      renderInspector();
      return Promise.resolve({ ok: true, command: command, output: state.lastCommandOutput });
    }

    setEngineStatus("Simulation: Executing command");
    addLog("CLI", "cli", "Magi> " + command);
    return fetch("/api/command", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ command: command })
    })
      .then(function (response) {
        if (!response.ok) throw new Error("HTTP " + response.status);
        return response.json();
      })
      .then(function (result) {
        state.lastCommand = command;
        state.lastCommandOutput = result.output || "Command completed without stdout output.";
        renderCommandOutput(result.output || "", result.ok);
        if (result.topology) {
          setTopology(result.topology, "live backend");
        }
        setEngineStatus(result.ok ? "Simulation: Ready" : "Simulation: Command returned error");
        if (result.ok) {
          animateCommandIfPossible(command);
        }
        renderInspector();
        return result;
      })
      .catch(function (error) {
        setEngineStatus("Simulation: Backend error");
        addLog("Backend", "error", error.message);
        state.lastCommand = command;
        state.lastCommandOutput = error.message;
        renderInspector();
        return { ok: false, command: command, output: error.message };
      });
  }

  function renderCommandOutput(output, ok) {
    var lines = String(output || "").split(/\r?\n/).filter(function (line) {
      return line.trim().length > 0;
    });
    if (!lines.length) {
      addLog("CLI", ok ? "cli" : "error", ok ? "Command completed" : "Command failed");
      return;
    }
    lines.slice(0, 24).forEach(function (line) {
      addLog("CLI", ok ? "cli" : "error", line);
    });
    if (lines.length > 24) {
      addLog("CLI", "cli", "Output truncated in event log. Use command again or check terminal for full detail.");
    }
  }

  function animateCommandIfPossible(command) {
    var parsed = parseCommand(command);
    var source = parsed.source;
    var target = parsed.target;
    if (source && target && source !== target) {
      animatePacket(source, target, parsed.protocol || "icmp");
    }
  }

  function simulateCommand(command) {
    var parsed = parseCommand(command);
    var source = parsed.source || (state.nodes[0] && state.nodes[0].name) || "System";
    var target = parsed.target || findLikelyPeer(source);
    var protocol = parsed.protocol || "icmp";

    addLog(source, protocol, command);
    animatePacket(source, target, protocol, function () {
      if (protocol === "icmp") addLog(target, protocol, "Reply from " + target + " received by " + source);
      if (protocol === "tcp") addLog(target, protocol, "SYN-ACK then ACK, state ESTABLISHED");
      if (protocol === "http") addLog(target, protocol, "HTTP/1.1 200 OK sent to " + source);
      if (protocol === "udp") addLog(target, protocol, "UDP datagram delivered");
      if (protocol === "arp") addLog(target, protocol, "ARP reply resolved MAC address");
    });
  }

  function parseCommand(command) {
    var parts = String(command || "").trim().split(/\s+/);
    if (!parts[0] || parts[0].toLowerCase() === "step") {
      var first = state.nodes[0] && state.nodes[0].name;
      return {
        source: first,
        target: findLikelyPeer(first),
        protocol: "icmp"
      };
    }
    var action = (parts[1] || parts[0] || "").toLowerCase();
    var protocolByAction = {
      ping: "icmp",
      traceroute: "icmp",
      tcp_connect: "tcp",
      tcp_close: "tcp",
      udp_send: "udp",
      http_get: "http",
      arp: "arp",
      dhcp_discover: "udp"
    };

    if (!protocolByAction[action]) {
      return { source: "", target: "", protocol: "" };
    }

    var target = "";
    if (action === "arp" || action === "dhcp_discover") {
      target = findLikelyPeer(parts[0]);
    } else {
      target = resolveTarget(parts[2] || parts[1]);
    }

    return {
      source: parts[0],
      target: target,
      protocol: protocolByAction[action]
    };
  }

  function resolveTarget(value) {
    if (!value) return "";
    var stripped = String(value).replace(/^https?:\/\//, "").split("/")[0];
    var byName = state.nodes.find(function (node) {
      return node.name === stripped || ("www." + node.name.toLowerCase() + ".com") === stripped.toLowerCase();
    });
    if (byName) return byName.name;
    var byIp = state.nodes.find(function (node) {
      return node.ip && node.ip.split("/")[0] === stripped;
    });
    return byIp ? byIp.name : "";
  }

  function findLikelyPeer(source) {
    var link = state.links.find(function (item) {
      return item.a === source || item.b === source;
    });
    if (link) return link.a === source ? link.b : link.a;
    var node = state.nodes.find(function (item) { return item.name !== source; });
    return node ? node.name : source;
  }

  function graphPathBetween(source, target) {
    if (!source || !target) return [];
    if (source === target) return [source];

    var queue = [[source]];
    var visited = {};
    visited[source] = true;

    while (queue.length) {
      var path = queue.shift();
      var current = path[path.length - 1];
      var neighbors = [];

      state.links.forEach(function (link) {
        if (link.a === current) neighbors.push(link.b);
        if (link.b === current) neighbors.push(link.a);
      });

      for (var i = 0; i < neighbors.length; i += 1) {
        var neighbor = neighbors[i];
        if (visited[neighbor]) continue;
        var nextPath = path.concat([neighbor]);
        if (neighbor === target) {
          return nextPath;
        }
        visited[neighbor] = true;
        queue.push(nextPath);
      }
    }

    return [source, target];
  }

  function packetWaypoints(source, target) {
    var nodePath = graphPathBetween(source, target);
    return nodePath.map(function (nodeId) {
      return state.positions[nodeId];
    }).filter(Boolean);
  }

  function animatePacket(source, target, protocol, done) {
    var waypoints = packetWaypoints(source, target);
    if (waypoints.length < 2) {
      addLog("System", "system", "No visual path for command; packet animation skipped");
      if (done) done();
      return;
    }

    state.packetCount += 1;
    updateCounters();

    var packet = document.createElement("div");
    packet.className = "packet " + protocol;
    packet.textContent = protocol.toUpperCase();
    packet.style.left = waypoints[0].x + "px";
    packet.style.top = waypoints[0].y + "px";
    packet.style.opacity = "0";
    els.canvasWrap.appendChild(packet);

    var speed = 1;
    var hopIndex = 0;

    function finishAnimation() {
      packet.classList.add("leaving");
      packet.style.opacity = "0";
      window.setTimeout(function () {
        packet.remove();
        if (done) done();
      }, 180);
    }

    function moveToNextHop() {
      if (hopIndex >= waypoints.length - 1) {
        finishAnimation();
        return;
      }

      var from = waypoints[hopIndex];
      var to = waypoints[hopIndex + 1];
      var dx = to.x - from.x;
      var dy = to.y - from.y;
      var distance = Math.sqrt((dx * dx) + (dy * dy));
      var duration = Math.max(220, Math.min(820, (distance * 5.4) / Math.max(speed, 0.1)));
      packet.style.transitionDuration = duration + "ms, " + duration + "ms, 120ms";
      packet.classList.add("moving");
      packet.style.left = to.x + "px";
      packet.style.top = to.y + "px";
      packet.style.opacity = "1";

      hopIndex += 1;
      window.setTimeout(moveToNextHop, duration + 20);
    }

    requestAnimationFrame(function () {
      packet.style.opacity = "1";
      moveToNextHop();
    });
  }

  function addLog(source, type, message) {
    var tr = document.createElement("tr");
    var time = new Date().toLocaleTimeString("en-GB", { hour12: false });
    tr.dataset.logType = String(type || "system").toLowerCase();
    tr.dataset.logSource = String(source || "");
    tr.innerHTML =
      "<td>" + time + "</td>" +
      "<td>" + escapeHtml(source) + "</td>" +
      '<td><span class="tag ' + escapeHtml(type) + '">' + escapeHtml(type.toUpperCase()) + "</span></td>" +
      "<td>" + escapeHtml(message) + "</td>";
    els.logBody.appendChild(tr);
    applyLogFilter();
    scrollLogToBottom();
  }

  function scrollLogToBottom() {
    var scrollWrap = els.logBody && els.logBody.closest(".table-scroll");
    if (scrollWrap) {
      scrollWrap.scrollTop = scrollWrap.scrollHeight;
    }
  }

  function addDemoNode() {
    var id = "H" + (state.nodes.filter(function (node) { return node.type === "host"; }).length + 1);
    state.topology.hosts = state.topology.hosts || [];
    state.topology.hosts.push({
      name: id,
      ip_address: "192.168.1." + (20 + state.topology.hosts.length) + "/24",
      default_gateway: "192.168.1.1"
    });
    if (state.topology.switches && state.topology.switches[0]) {
      state.topology.links = state.topology.links || [];
      state.topology.links.push({ endpoints: [id, state.topology.switches[0].name + ":3"], delay: 5 });
    }
    setTopology(state.topology, state.source);
    addLog("System", "system", "Added demo host " + id + " to dashboard model");
  }

  function downloadSnapshot() {
    if (state.apiAvailable) {
      executeCommand("save topology.json");
      return;
    }
    var data = JSON.stringify(state.topology, null, 2);
    var blob = new Blob([data], { type: "application/json" });
    var url = URL.createObjectURL(blob);
    var a = document.createElement("a");
    a.href = url;
    a.download = "magi-dashboard-topology.json";
    a.click();
    URL.revokeObjectURL(url);
    addLog("System", "system", "Downloaded dashboard topology snapshot");
  }

  function updateCounters() {
    els.packetCounter.textContent = "Packets: " + state.packetCount;
  }

  function setEngineStatus(text) {
    els.engineStatus.textContent = text;
  }

  function deviceKind(node) {
    if (!node) return "switch";
    if (node.type === "host" && String(node.name || "").indexOf("SRV") === 0) return "server";
    if (node.type === "router" && String(node.name || "").indexOf("FW") === 0) return "firewall";
    return node.type;
  }

  function deviceArtwork(node) {
    var kind = deviceKind(node);
    if (kind === "host") return hostArtwork();
    if (kind === "server") return serverArtwork();
    if (kind === "router") return routerArtwork();
    if (kind === "firewall") return firewallArtwork();
    return switchArtwork();
  }

  function switchArtwork() {
    return [
      '<div class="device-artwork">',
      '<svg class="device-svg" viewBox="0 0 170 110" aria-hidden="true">',
      '<ellipse cx="85" cy="98" rx="54" ry="8" fill="rgba(15,23,42,0.12)"></ellipse>',
      '<rect x="18" y="34" width="134" height="18" rx="9" fill="#d5dde6"></rect>',
      '<rect x="12" y="44" width="146" height="30" rx="10" fill="#475569"></rect>',
      '<rect x="18" y="48" width="134" height="24" rx="8" fill="#334155"></rect>',
      devicePortStrip(26, 55, 12, 8, 9, 4, "#111827", "#6b7280"),
      deviceLedStrip(116, 58, 4, 9, ["#22c55e", "#38bdf8", "#f59e0b", "#22c55e"]),
      '<rect x="22" y="52" width="20" height="14" rx="3" fill="#64748b"></rect>',
      '<rect x="24" y="55" width="16" height="2" rx="1" fill="rgba(255,255,255,0.44)"></rect>',
      '</svg>',
      '</div>'
    ].join("");
  }

  function routerArtwork() {
    return [
      '<div class="device-artwork">',
      '<svg class="device-svg" viewBox="0 0 170 110" aria-hidden="true">',
      '<ellipse cx="85" cy="98" rx="52" ry="8" fill="rgba(15,23,42,0.12)"></ellipse>',
      '<path d="M24 66 L36 32 H134 L146 66 Z" fill="#7b8ea6"></path>',
      '<rect x="18" y="54" width="134" height="24" rx="9" fill="#a9b8c9"></rect>',
      '<rect x="26" y="58" width="118" height="16" rx="6" fill="#e7edf3"></rect>',
      '<rect x="32" y="61" width="34" height="10" rx="4" fill="#64748b"></rect>',
      '<rect x="72" y="61" width="20" height="10" rx="3" fill="#0f172a"></rect>',
      '<rect x="96" y="61" width="20" height="10" rx="3" fill="#0f172a"></rect>',
      '<rect x="120" y="61" width="16" height="10" rx="3" fill="#38bdf8"></rect>',
      '<circle cx="42" cy="40" r="4" fill="#38bdf8"></circle>',
      '<circle cx="128" cy="40" r="4" fill="#22c55e"></circle>',
      '</svg>',
      '</div>'
    ].join("");
  }

  function firewallArtwork() {
    return [
      '<div class="device-artwork">',
      '<svg class="device-svg" viewBox="0 0 170 110" aria-hidden="true">',
      '<ellipse cx="85" cy="98" rx="54" ry="8" fill="rgba(15,23,42,0.14)"></ellipse>',
      '<rect x="14" y="44" width="142" height="32" rx="11" fill="#2b3440"></rect>',
      '<rect x="20" y="49" width="130" height="22" rx="8" fill="#111827"></rect>',
      '<rect x="20" y="49" width="30" height="22" rx="8" fill="#7f1d1d"></rect>',
      '<path d="M35 54 L42 57 L42 63 C42 67 39 70 35 72 C31 70 28 67 28 63 L28 57 Z" fill="#fee2e2"></path>',
      devicePortStrip(58, 56, 8, 9, 10, 4, "#0f172a", "#6b7280"),
      deviceLedStrip(122, 58, 3, 9, ["#ef4444", "#f59e0b", "#22c55e"]),
      '<rect x="56" y="50" width="58" height="3" rx="1.5" fill="rgba(255,255,255,0.08)"></rect>',
      '</svg>',
      '</div>'
    ].join("");
  }

  function serverArtwork() {
    return [
      '<div class="device-artwork">',
      '<svg class="device-svg" viewBox="0 0 170 110" aria-hidden="true">',
      '<ellipse cx="85" cy="98" rx="34" ry="7" fill="rgba(15,23,42,0.12)"></ellipse>',
      '<rect x="58" y="16" width="54" height="78" rx="11" fill="#cfd8e3"></rect>',
      '<rect x="64" y="22" width="42" height="66" rx="8" fill="#1f2937"></rect>',
      '<rect x="70" y="30" width="30" height="10" rx="3" fill="#4b5563"></rect>',
      '<rect x="70" y="44" width="30" height="10" rx="3" fill="#4b5563"></rect>',
      '<rect x="70" y="58" width="30" height="10" rx="3" fill="#4b5563"></rect>',
      '<rect x="70" y="72" width="18" height="8" rx="3" fill="#64748b"></rect>',
      '<circle cx="95" cy="76" r="4" fill="#22c55e"></circle>',
      '<rect x="72" y="25" width="26" height="2" rx="1" fill="rgba(255,255,255,0.32)"></rect>',
      '</svg>',
      '</div>'
    ].join("");
  }

  function hostArtwork() {
    return [
      '<div class="device-artwork">',
      '<svg class="device-svg" viewBox="0 0 170 110" aria-hidden="true">',
      '<ellipse cx="85" cy="100" rx="46" ry="7" fill="rgba(15,23,42,0.12)"></ellipse>',
      '<rect x="24" y="16" width="122" height="68" rx="13" fill="#d7dfe8"></rect>',
      '<rect x="30" y="22" width="110" height="56" rx="10" fill="#111827"></rect>',
      '<rect x="36" y="28" width="98" height="44" rx="7" fill="#124e78"></rect>',
      '<path d="M36 60 Q70 36 134 44 L134 72 L36 72 Z" fill="rgba(255,255,255,0.08)"></path>',
      '<circle cx="85" cy="77" r="2.2" fill="#94a3b8"></circle>',
      '<rect x="76" y="84" width="18" height="10" rx="3" fill="#a8b5c4"></rect>',
      '<rect x="57" y="94" width="56" height="8" rx="4" fill="#c5d0da"></rect>',
      '</svg>',
      '</div>'
    ].join("");
  }

  function devicePortStrip(startX, y, count, step, width, height, fill, stroke) {
    var parts = [];
    for (var i = 0; i < count; i += 1) {
      var x = startX + (i * step);
      parts.push('<rect x="' + x + '" y="' + y + '" width="' + width + '" height="' + height + '" rx="1.8" fill="' + fill + '" stroke="' + stroke + '" stroke-width="0.8"></rect>');
    }
    return parts.join("");
  }

  function deviceLedStrip(startX, y, count, step, colors) {
    var parts = [];
    for (var i = 0; i < count; i += 1) {
      var x = startX + (i * step);
      parts.push('<circle cx="' + x + '" cy="' + y + '" r="2.6" fill="' + (colors[i] || "#22c55e") + '"></circle>');
    }
    return parts.join("");
  }

  function iconFor(type, name) {
    if (type === "host" && String(name || "").indexOf("SRV") === 0) return "dns";
    if (type === "router" && String(name || "").indexOf("FW") === 0) return "security";
    if (type === "host") return "desktop_windows";
    if (type === "switch") return "hub";
    if (type === "router") return "router";
    if (type === "server") return "dns";
    return "device_hub";
  }

  function svg(name, attrs) {
    var el = document.createElementNS("http://www.w3.org/2000/svg", name);
    Object.keys(attrs).forEach(function (key) {
      el.setAttribute(key, attrs[key]);
    });
    return el;
  }

  function escapeHtml(value) {
    return String(value == null ? "" : value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#039;");
  }
})();
