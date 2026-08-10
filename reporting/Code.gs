const SPREADSHEET_ID = '1eN1xMtflFG_JApo1zN1U3ok6pX4ES9lzyDhlHv65VGM';
const SHEET_NAME = '시트1';
const TELEMETRY_SHEET_NAME = '시트2';
const TELEMETRY_COLUMN_COUNT = 14;
const MAX_REQUEST_BYTES = 4096;
const TELEMETRY_ACTIVE_DAYS = 30;
const TELEMETRY_RETENTION_DAYS = 90;
const TELEMETRY_STATS_CACHE_KEY = 'telemetry-stats-v3';
const TELEMETRY_STATS_CACHE_SECONDS = 1800;
const TELEMETRY_PLUGIN_IDS = [
  'tb_center',
  'tb_compressor',
  'tb_distortion',
  'tb_disperser',
  'tb_eq',
  'tb_colorizer',
  'tb_inverted_flanger',
  'tb_inverted_phaser',
  'tb_jewel_digger',
  'tb_noise_remover',
  'tb_parallel_reverb',
  'tb_scrambler',
  'tb_transient_shaper',
  'tb_step_shifter',
  'tb_tune',
  'tb_volume',
  'tb_vocoder',
  'tb_xyz_panner',
  'tb_limiter',
  'tb_sublow',
  'tb_delay',
  'tb_pitch_shifter',
  'tb_resonator',
  'tb_ring_modulation',
  'tb_recorder',
  'tb_shimmer',
  'tb_exciter',
  'tb_tape',
];

function doGet(event) {
  const action = cleanText(event && event.parameter && event.parameter.action, 40);
  if (action === 'stats') {
    return handleTelemetryStats();
  }

  return jsonResponse({ ok: true, status: 'ready' });
}

function doPost(event) {
  const contents = event && event.postData && event.postData.contents;
  const contentType = String(event && event.postData && event.postData.type || '')
    .toLowerCase();
  const requestLength = Number(
    event && (event.contentLength || event.postData && event.postData.length)
    || contents && contents.length
    || 0);

  if (typeof contents !== 'string'
      || !contentType.startsWith('application/json')
      || !Number.isFinite(requestLength)
      || requestLength > MAX_REQUEST_BYTES) {
    return jsonResponse({ ok: false, error: 'Invalid request size.' });
  }

  let payload;
  try {
    payload = JSON.parse(contents);
  } catch (error) {
    return jsonResponse({ ok: false, error: 'Invalid JSON.' });
  }

  if (typeof payload.kind === 'string' && payload.kind.startsWith('telemetry.')) {
    return handleTelemetry(payload);
  }

  return handleBugReport(payload);
}

function handleBugReport(payload) {
  const reportId = cleanText(payload.reportId, 64);
  const pluginName = cleanText(payload.pluginName, 100);
  const report = cleanText(payload.report, 600);
  const hubVersion = cleanText(payload.hubVersion, 30);
  const pluginVersion = cleanText(payload.pluginVersion, 30);
  const os = cleanText(payload.os, 160);

  if (payload.schemaVersion !== 1 || !reportId || !pluginName || !report) {
    return jsonResponse({ ok: false, error: 'Missing required fields.' });
  }

  const assessment = assessReport(report, pluginName);
  if (!assessment.shouldStore) {
    return jsonResponse({ ok: true, status: 'created' });
  }

  const fingerprint = sha256Hex(
    normalizeForDuplicate(pluginName) + '\n' + normalizeForDuplicate(report));
  const lock = LockService.getScriptLock();

  try {
    if (!lock.tryLock(250)) {
      return jsonResponse({ ok: true, status: 'created' });
    }

    const sheet = SpreadsheetApp.openById(SPREADSHEET_ID).getSheetByName(SHEET_NAME);
    if (!sheet) {
      return jsonResponse({ ok: false, error: 'Report sheet not found.' });
    }

    const lastRow = sheet.getLastRow();
    if (lastRow > 1) {
      const fingerprintCell = sheet
        .getRange(2, 10, lastRow - 1, 1)
        .createTextFinder(fingerprint)
        .matchEntireCell(true)
        .findNext();

      if (fingerprintCell) {
        return jsonResponse({
          ok: true,
          status: 'duplicate',
          ticketId: sheet.getRange(fingerprintCell.getRow(), 5).getDisplayValue(),
        });
      }
    }

    const now = new Date();
    const ticketId =
      'TB-' +
      Utilities.formatDate(now, 'Asia/Seoul', 'yyyyMMdd') +
      '-' +
      Utilities.getUuid().replace(/-/g, '').slice(0, 6).toUpperCase();

    sheet.appendRow([
      safeCellText(pluginName),
      safeCellText(report),
      now,
      assessment.importance,
      ticketId,
      'Open',
      safeCellText(hubVersion || 'Unknown'),
      safeCellText(pluginVersion || 'Unknown'),
      safeCellText(os || 'Unknown'),
      fingerprint,
    ]);

    sheet.getRange(sheet.getLastRow(), 3).setNumberFormat('yyyy-mm-dd hh:mm:ss');
    return jsonResponse({ ok: true, status: 'created', ticketId: ticketId });
  } catch (error) {
    return jsonResponse({ ok: false, error: 'Could not save the report.' });
  } finally {
    if (lock.hasLock()) {
      lock.releaseLock();
    }
  }
}

function handleTelemetry(payload) {
  if (payload.schemaVersion !== 1) {
    return jsonResponse({ ok: false, error: 'Unsupported telemetry schema.' });
  }

  const token = cleanText(payload.token, 80);
  if (!/^[A-Za-z0-9_-]{43,64}$/.test(token)) {
    return jsonResponse({ ok: false, error: 'Invalid installation token.' });
  }

  const lock = LockService.getScriptLock();
  try {
    if (!lock.tryLock(500)) {
      return jsonResponse({ ok: false, error: 'Telemetry service is busy.' });
    }

    const sheet = SpreadsheetApp
      .openById(SPREADSHEET_ID)
      .getSheetByName(TELEMETRY_SHEET_NAME);
    if (!sheet) {
      return jsonResponse({ ok: false, error: 'Telemetry sheet not found.' });
    }
    ensureTelemetrySheetSchema(sheet);

    const tokenHash = sha256Hex(token);
    const kind = cleanText(payload.kind, 40);
    if (kind === 'telemetry.sync') {
      return handleTelemetrySync(sheet, tokenHash, payload);
    }
    if (kind === 'telemetry.like') {
      return handleTelemetryLike(sheet, tokenHash, payload);
    }
    if (kind === 'telemetry.uninstall') {
      return handleTelemetryUninstall(sheet, tokenHash, payload);
    }

    return jsonResponse({ ok: false, error: 'Unsupported telemetry action.' });
  } catch (error) {
    return jsonResponse({ ok: false, error: 'Could not update telemetry.' });
  } finally {
    if (lock.hasLock()) {
      lock.releaseLock();
    }
  }
}

function handleTelemetrySync(sheet, tokenHash, payload) {
  const now = new Date();
  const installedPlugins = sanitizeInstalledPlugins(payload.installedPlugins);
  const hubVersion = cleanText(payload.hubVersion, 30) || 'Unknown';
  const platform = cleanText(payload.os || payload.platform, 100) || 'Unknown';
  const requestId = cleanText(payload.requestId, 64);
  const consentVersion = cleanText(payload.consentVersion, 20) || '1';
  let rowNumber = findTelemetryRow(sheet, tokenHash);
  let row;

  if (rowNumber === 0) {
    row = [
      tokenHash,
      now,
      now,
      true,
      safeCellText(hubVersion),
      safeCellText(platform),
      JSON.stringify(installedPlugins),
      '[]',
      '{}',
      '',
      requestId,
      1,
      safeCellText(consentVersion),
      '[]',
    ];
    sheet.appendRow(row);
    rowNumber = sheet.getLastRow();
  } else {
    row = sheet.getRange(rowNumber, 1, 1, TELEMETRY_COLUMN_COUNT).getValues()[0];
    const previousInstalled = sanitizeInstalledPlugins(parseJsonObject(row[6]));
    row[13] = JSON.stringify(reconcileUninstalledPlugins(
      previousInstalled,
      installedPlugins,
      parseJsonArray(row[13])));
    row[1] = toValidDate(row[1]) || now;
    row[2] = now;
    row[3] = true;
    row[4] = safeCellText(hubVersion);
    row[5] = safeCellText(platform);
    row[6] = JSON.stringify(installedPlugins);
    row[7] = JSON.stringify(sanitizeLikedPlugins(parseJsonArray(row[7])));
    row[8] = JSON.stringify(sanitizeLikedAt(parseJsonObject(row[8])));
    row[9] = '';
    row[10] = requestId;
    row[11] = 1;
    row[12] = safeCellText(consentVersion);
    sheet.getRange(rowNumber, 1, 1, TELEMETRY_COLUMN_COUNT).setValues([row]);
  }

  formatTelemetryDates(sheet, rowNumber);
  purgeStaleTelemetryRowsIfDue(sheet, now);
  invalidateTelemetryStats();

  return jsonResponse({
    ok: true,
    status: 'synced',
    likedPluginIds: sanitizeLikedPlugins(parseJsonArray(row[7])),
  });
}

function handleTelemetryLike(sheet, tokenHash, payload) {
  const pluginId = cleanText(payload.pluginId, 80);
  const requestId = cleanText(payload.requestId, 64);
  if (!isAllowedPluginId(pluginId)
      || typeof payload.liked !== 'boolean'
      || !requestId) {
    return jsonResponse({ ok: false, error: 'Invalid like request.' });
  }

  const now = new Date();
  const incomingInstalled = sanitizeInstalledPlugins(payload.installedPlugins);
  const hubVersion = cleanText(payload.hubVersion, 30) || 'Unknown';
  const platform = cleanText(payload.os || payload.platform, 100) || 'Unknown';
  const consentVersion = cleanText(payload.consentVersion, 20) || '1';
  let rowNumber = findTelemetryRow(sheet, tokenHash);
  let row;

  if (rowNumber === 0) {
    row = [
      tokenHash,
      now,
      now,
      true,
      safeCellText(hubVersion),
      safeCellText(platform),
      JSON.stringify(incomingInstalled),
      '[]',
      '{}',
      '',
      '',
      1,
      safeCellText(consentVersion),
      '[]',
    ];
    sheet.appendRow(row);
    rowNumber = sheet.getLastRow();
  } else {
    row = sheet.getRange(rowNumber, 1, 1, TELEMETRY_COLUMN_COUNT).getValues()[0];
  }

  if (cleanText(row[10], 64) === requestId) {
    const duplicateLikes = sanitizeLikedPlugins(parseJsonArray(row[7]));
    return jsonResponse({
      ok: true,
      status: 'duplicate',
      liked: duplicateLikes.includes(pluginId),
      likedPluginIds: duplicateLikes,
    });
  }

  const installedPlugins = Object.keys(incomingInstalled).length > 0
    ? incomingInstalled
    : sanitizeInstalledPlugins(parseJsonObject(row[6]));
  if (payload.liked
      && !Object.prototype.hasOwnProperty.call(installedPlugins, pluginId)) {
    return jsonResponse({ ok: false, error: 'Install the plug-in before liking it.' });
  }

  const likedPlugins = sanitizeLikedPlugins(parseJsonArray(row[7]));
  const likedAt = sanitizeLikedAt(parseJsonObject(row[8]));
  const previousInstalled = sanitizeInstalledPlugins(parseJsonObject(row[6]));
  const uninstalledPlugins = reconcileUninstalledPlugins(
    previousInstalled,
    installedPlugins,
    parseJsonArray(row[13]));
  const existingIndex = likedPlugins.indexOf(pluginId);

  if (payload.liked && existingIndex === -1) {
    likedPlugins.push(pluginId);
    likedAt[pluginId] = now.toISOString();
  } else if (!payload.liked && existingIndex !== -1) {
    likedPlugins.splice(existingIndex, 1);
    delete likedAt[pluginId];
  }

  likedPlugins.sort();
  row[1] = toValidDate(row[1]) || now;
  row[2] = now;
  row[3] = true;
  row[4] = safeCellText(hubVersion);
  row[5] = safeCellText(platform);
  row[6] = JSON.stringify(installedPlugins);
  row[7] = JSON.stringify(likedPlugins);
  row[8] = JSON.stringify(likedAt);
  row[9] = '';
  row[10] = requestId;
  row[11] = 1;
  row[12] = safeCellText(consentVersion);
  row[13] = JSON.stringify(uninstalledPlugins);
  sheet.getRange(rowNumber, 1, 1, TELEMETRY_COLUMN_COUNT).setValues([row]);

  formatTelemetryDates(sheet, rowNumber);
  purgeStaleTelemetryRowsIfDue(sheet, now);
  invalidateTelemetryStats();

  return jsonResponse({
    ok: true,
    status: 'updated',
    liked: payload.liked,
    likedPluginIds: likedPlugins,
  });
}

function handleTelemetryUninstall(sheet, tokenHash, payload) {
  const rowNumber = findTelemetryRow(sheet, tokenHash);
  if (rowNumber === 0) {
    return jsonResponse({ ok: true, status: 'not_found' });
  }

  sheet.deleteRow(rowNumber);
  invalidateTelemetryStats();
  return jsonResponse({ ok: true, status: 'deleted' });
}

function handleTelemetryStats() {
  const cache = CacheService.getScriptCache();
  const cached = cache.get(TELEMETRY_STATS_CACHE_KEY);
  if (cached) {
    try {
      return jsonResponse(JSON.parse(cached));
    } catch (error) {
      cache.remove(TELEMETRY_STATS_CACHE_KEY);
    }
  }

  try {
    const sheet = SpreadsheetApp
      .openById(SPREADSHEET_ID)
      .getSheetByName(TELEMETRY_SHEET_NAME);
    if (!sheet) {
      return jsonResponse({ ok: false, error: 'Telemetry sheet not found.' });
    }

    const now = new Date();
    const activeCutoff = new Date(
      now.getTime() - TELEMETRY_ACTIVE_DAYS * 24 * 60 * 60 * 1000);
    const stats = {};
    TELEMETRY_PLUGIN_IDS.forEach(pluginId => {
      stats[pluginId] = {
        installedActive: 0,
        uninstalledActive: 0,
        likedActive: 0,
        likedInstalledActive: 0,
      };
    });

    let activeHubCount = 0;
    const lastRow = sheet.getLastRow();
    if (lastRow > 1) {
      const readableColumnCount = Math.min(
        TELEMETRY_COLUMN_COUNT,
        sheet.getMaxColumns());
      const rows = sheet
        .getRange(2, 1, lastRow - 1, readableColumnCount)
        .getValues();

      rows.forEach(row => {
        const lastSeen = toValidDate(row[2]);
        if (!isTrue(row[3]) || !lastSeen || lastSeen < activeCutoff) {
          return;
        }

        activeHubCount++;
        const installed = sanitizeInstalledPlugins(parseJsonObject(row[6]));
        const uninstalled = sanitizeUninstalledPlugins(parseJsonArray(row[13]));
        const liked = sanitizeLikedPlugins(parseJsonArray(row[7]));
        const likedLookup = {};
        liked.forEach(pluginId => {
          likedLookup[pluginId] = true;
          stats[pluginId].likedActive++;
        });

        Object.keys(installed).forEach(pluginId => {
          stats[pluginId].installedActive++;
          if (likedLookup[pluginId]) {
            stats[pluginId].likedInstalledActive++;
          }
        });
        uninstalled.forEach(pluginId => {
          stats[pluginId].uninstalledActive++;
        });
      });
    }

    const ranked = [];
    const pluginStats = {};
    TELEMETRY_PLUGIN_IDS.forEach(pluginId => {
      const counts = stats[pluginId];
      const installCount = counts.installedActive;
      const likeCount = counts.likedActive;
      const likedInstalled = counts.likedInstalledActive;
      const likeRate = installCount > 0 ? likedInstalled / installCount : 0;
      const score = installCount >= 5
        ? wilsonLowerBound(likedInstalled, installCount)
        : null;

      pluginStats[pluginId] = {
        rank: null,
        likeCount: likeCount,
        installCount: installCount,
        uninstallCount: counts.uninstalledActive,
        likeRate: likeRate,
        installRate: activeHubCount > 0 ? installCount / activeHubCount : 0,
        likeReach: activeHubCount > 0 ? likeCount / activeHubCount : 0,
        score: score,
        sampleEnough: installCount >= 5,
      };

      if (score !== null) {
        ranked.push({ pluginId: pluginId, score: score, likeCount: likeCount });
      }
    });

    ranked.sort((left, right) =>
      right.score - left.score
      || right.likeCount - left.likeCount
      || left.pluginId.localeCompare(right.pluginId));
    ranked.forEach((entry, index) => {
      pluginStats[entry.pluginId].rank = index + 1;
    });

    const response = {
      ok: true,
      generatedAt: now.toISOString(),
      activeHubCount: activeHubCount,
      activeWindowDays: TELEMETRY_ACTIVE_DAYS,
      plugins: pluginStats,
    };
    cache.put(
      TELEMETRY_STATS_CACHE_KEY,
      JSON.stringify(response),
      TELEMETRY_STATS_CACHE_SECONDS);
    return jsonResponse(response);
  } catch (error) {
    return jsonResponse({ ok: false, error: 'Could not load telemetry stats.' });
  }
}

function findTelemetryRow(sheet, tokenHash) {
  const lastRow = sheet.getLastRow();
  if (lastRow <= 1) {
    return 0;
  }

  const match = sheet
    .getRange(2, 1, lastRow - 1, 1)
    .createTextFinder(tokenHash)
    .matchEntireCell(true)
    .findNext();
  return match ? match.getRow() : 0;
}

function ensureTelemetrySheetSchema(sheet) {
  const missingColumns = TELEMETRY_COLUMN_COUNT - sheet.getMaxColumns();
  if (missingColumns > 0) {
    sheet.insertColumnsAfter(sheet.getMaxColumns(), missingColumns);
  }

  const header = sheet.getRange(1, TELEMETRY_COLUMN_COUNT);
  if (!cleanText(header.getDisplayValue(), 100)) {
    header.setValue('UninstalledPluginIdsJson');
  }
}

function sanitizeInstalledPlugins(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return {};
  }

  const installed = {};
  Object.keys(value).slice(0, TELEMETRY_PLUGIN_IDS.length).forEach(pluginId => {
    if (!isAllowedPluginId(pluginId)) {
      return;
    }

    const version = cleanText(value[pluginId], 30);
    if (version) {
      installed[pluginId] = version;
    }
  });
  return installed;
}

function sanitizeLikedPlugins(value) {
  if (!Array.isArray(value)) {
    return [];
  }

  return Array.from(new Set(
    value
      .map(pluginId => cleanText(pluginId, 80))
      .filter(isAllowedPluginId)))
    .sort();
}

function sanitizeUninstalledPlugins(value) {
  return sanitizeLikedPlugins(value);
}

function reconcileUninstalledPlugins(previousInstalled, installed, value) {
  const uninstalled = new Set(sanitizeUninstalledPlugins(value));

  Object.keys(previousInstalled).forEach(pluginId => {
    if (!Object.prototype.hasOwnProperty.call(installed, pluginId)) {
      uninstalled.add(pluginId);
    }
  });
  Object.keys(installed).forEach(pluginId => {
    uninstalled.delete(pluginId);
  });

  return Array.from(uninstalled).sort();
}

function sanitizeLikedAt(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return {};
  }

  const likedAt = {};
  Object.keys(value).forEach(pluginId => {
    if (!isAllowedPluginId(pluginId)) {
      return;
    }

    const timestamp = cleanText(value[pluginId], 40);
    if (timestamp) {
      likedAt[pluginId] = timestamp;
    }
  });
  return likedAt;
}

function isAllowedPluginId(pluginId) {
  return TELEMETRY_PLUGIN_IDS.includes(pluginId);
}

function parseJsonObject(value) {
  try {
    const parsed = JSON.parse(String(value || '{}'));
    return parsed && typeof parsed === 'object' && !Array.isArray(parsed)
      ? parsed
      : {};
  } catch (error) {
    return {};
  }
}

function parseJsonArray(value) {
  try {
    const parsed = JSON.parse(String(value || '[]'));
    return Array.isArray(parsed) ? parsed : [];
  } catch (error) {
    return [];
  }
}

function toValidDate(value) {
  const date = value instanceof Date ? value : new Date(value);
  return Number.isNaN(date.getTime()) ? null : date;
}

function isTrue(value) {
  return value === true || String(value).toLowerCase() === 'true';
}

function wilsonLowerBound(successes, total) {
  if (total <= 0) {
    return 0;
  }

  const z = 1.96;
  const proportion = successes / total;
  const zSquared = z * z;
  const denominator = 1 + zSquared / total;
  const centre = proportion + zSquared / (2 * total);
  const margin = z * Math.sqrt(
    (proportion * (1 - proportion) + zSquared / (4 * total)) / total);
  return (centre - margin) / denominator;
}

function formatTelemetryDates(sheet, rowNumber) {
  sheet.getRange(rowNumber, 2, 1, 2).setNumberFormat('yyyy-mm-dd hh:mm:ss');
  sheet.getRange(rowNumber, 10).setNumberFormat('yyyy-mm-dd hh:mm:ss');
}

function purgeStaleTelemetryRowsIfDue(sheet, now) {
  const properties = PropertiesService.getScriptProperties();
  const today = Utilities.formatDate(now, 'Asia/Seoul', 'yyyy-MM-dd');
  if (properties.getProperty('telemetryLastPurgeDate') === today) {
    return;
  }

  const cutoff = new Date(
    now.getTime() - TELEMETRY_RETENTION_DAYS * 24 * 60 * 60 * 1000);
  const lastRow = sheet.getLastRow();
  if (lastRow > 1) {
    const lastSeenValues = sheet.getRange(2, 3, lastRow - 1, 1).getValues();
    for (let index = lastSeenValues.length - 1; index >= 0; index--) {
      const lastSeen = toValidDate(lastSeenValues[index][0]);
      if (!lastSeen || lastSeen < cutoff) {
        sheet.deleteRow(index + 2);
      }
    }
  }

  properties.setProperty('telemetryLastPurgeDate', today);
}

function invalidateTelemetryStats() {
  CacheService.getScriptCache().remove(TELEMETRY_STATS_CACHE_KEY);
}

function assessReport(report, pluginName) {
  return {
    shouldStore: !getRejectionReason(report, pluginName),
    importance: classifyImportance(report),
  };
}

function getRejectionReason(report, pluginName) {
  const text = normalizeForDuplicate(report);
  const usefulShortReports = [
    '튕김', '무음', '렉', '안됨', '먹통', '크래시',
    'crash', 'freeze', 'frozen', 'hang', 'silent', 'no sound',
  ];

  if (usefulShortReports.includes(text)) {
    return '';
  }

  const placeholders = [
    'test', 'testing', '테스트', '테스트입니다', 'asdf', 'asdfg',
    'qwer', 'qwerty', 'zxcv', 'zxcvbn', 'ㅁㄴㅇㄹ', 'ㅋㅋ', 'ㅎㅎ',
    'ㅠㅠ', 'ㅜㅜ', 'ㅇㅇ', 'ㄴㄴ', 'none', 'nothing', 'n/a', 'na',
    'idk', 'no idea', '없음', '없어요', '몰라', '모름',
    '버그 내용을 입력하세요', 'describe the bug',
  ];

  if (placeholders.some(value => text === normalizeForDuplicate(value))) {
    return 'Please describe the problem instead of using test or placeholder text.';
  }

  const problemSignals = [
    'bug', 'error', 'issue', 'problem', 'crash', 'freeze', 'frozen',
    'fail', 'broken', 'glitch', 'hang', 'lag', 'latency', 'noise',
    'silent', 'slow', 'stutter', 'dropout', 'no sound', 'not work',
    "doesn't", 'does not', "won't",
    'cannot', "can't", 'unable', 'missing', 'incorrect',
    '버그', '오류', '에러', '문제', '크래시', '튕', '멈춤', '먹통',
    '실패', '안됨', '안 돼', '안돼', '안 되', '되지 않', '눌리지',
    '열리지', '실행 안', '소리 안', '무음', '깨짐', '이상', '잡음',
    '노이즈', '지연', '렉', '느려', '느림', '버벅', '끊김', '충돌',
    '사라짐', '작동 안',
  ];
  const hasProblemSignal = problemSignals.some(signal => text.includes(signal));
  const testOnlySignals = [
    '테스트로 보냅니다', '테스트로 보내요', '테스트 전송', '테스트입니다',
    '테스트용', 'test report', 'test message', 'sending a test', 'just testing',
  ];
  const praiseSignals = [
    '짱', '좋아요', '조아요', '좋습니다', '좋네요', '최고', '굿',
    '대박', 'awesome', 'great', 'nice', 'love it', 'thank you', 'thanks',
  ];

  if (!hasProblemSignal
      && (testOnlySignals.some(signal => text.includes(signal))
          || praiseSignals.some(signal => text.includes(signal)))) {
    return 'Please report a specific problem rather than a test or compliment.';
  }

  if (text === normalizeForDuplicate(pluginName)) {
    return 'Please describe what happened, not only the plug-in name.';
  }

  const meaningful = Array.from(text.match(/[\p{L}\p{N}]/gu) || []);
  if (meaningful.length === 0) {
    return 'Please add a short description of what went wrong.';
  }

  if (meaningful.length >= 2 && meaningful.every(char => char === meaningful[0])) {
    return 'Please add a few details so the report can be understood.';
  }

  if (meaningful.length === 1 && /^[a-z0-9]$/i.test(meaningful[0])) {
    return 'Please add a short description of what went wrong.';
  }

  return '';
}

function classifyImportance(report) {
  const text = normalizeForDuplicate(report);
  const highSignals = [
    'crash', 'crashed', 'freeze', 'frozen', 'hang', 'data loss',
    'will not open', "won't open", 'no sound', 'silent', 'install failed',
    '크래시', '튕', '먹통', '멈춤', '데이터 손실', '실행 안',
    '실행불가', '소리 안', '무음', '설치 실패',
  ];
  const lowSignals = [
    'typo', 'spelling', 'color', 'icon', 'alignment', 'suggestion',
    'feature request', '오타', '맞춤법', '색상', '아이콘', '정렬',
    '제안', '기능 요청',
  ];

  if (highSignals.some(signal => text.includes(signal))) {
    return 'High';
  }
  if (lowSignals.some(signal => text.includes(signal))) {
    return 'Low';
  }
  return 'Normal';
}

function cleanText(value, maxLength) {
  return String(value || '')
    .replace(/\r\n?/g, '\n')
    .trim()
    .slice(0, maxLength);
}

function normalizeForDuplicate(value) {
  return String(value || '')
    .normalize('NFKC')
    .replace(/\s+/g, ' ')
    .trim()
    .toLowerCase();
}

function safeCellText(value) {
  const text = String(value || '');
  return /^[=+\-@]/.test(text) ? "'" + text : text;
}

function sha256Hex(value) {
  return Utilities.computeDigest(
    Utilities.DigestAlgorithm.SHA_256,
    value,
    Utilities.Charset.UTF_8)
    .map(byte => (byte + 256).toString(16).slice(-2))
    .join('');
}

function jsonResponse(value) {
  return ContentService
    .createTextOutput(JSON.stringify(value))
    .setMimeType(ContentService.MimeType.JSON);
}
