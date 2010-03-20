<?php //$Id: qb_data.wc.inc,v 1.3 2009/02/17 20:44:29 vauxia Exp $

function _qb_data_qbwc_request() {

  // Update all of the lists we're watching.
  $res = db_query("SELECT * FROM {qb_data} 
    WHERE status = 1 
    AND frequency >= 0 AND frequency + lastrun < %d ORDER BY weight", 
    qbwc_get('time'));

  while ($row = db_fetch_object($res)) {
    // Import new values from Quickbooks into Drupal.
    if ($row->qb_qry) {
      $req = array(
        'name' => $row->request .'QueryRq',
        'callback' => array('qb_data_qbwc_response'),
        'data' => array('qb_query' => $row, 'op' => 'qry'),
        'request' => array(),
      );
      // TODO "request" key contains the actual filters.

      drupal_alter('qb_data_request', $req);

      $requests[] = $req;
    }

    // Import new or modified values from Drupal into Quickbooks.
    if ($row->qb_add || $row->qb_mod) {

      // Account for individual record changes.
      $res2 = db_query("SELECT * FROM {qb_data_value} 
        WHERE status = 1 AND qb_list_id is NULL");

      while ($val = db_fetch_object($res2)) {
        $request = array();

        if ($row->qb_add && is_null($val->qb_edit_sequence)) {
          $op = 'add';
          $name = $row->request .'AddRq';
          $key = $row->request .'Add';
        }
        elseif ($row->qb_mod) {
          $op = 'mod';
          $name = $row->request .'ModRq';
          $key = $row->request .'Mod';
          $request[$key]['EditSequence'] = $val->qb_edit_sequence;
          if ($val->qb_list_id) $request[$key]['ListID'] = $val->qb_list_id;
          if ($val->qb_txn_id) $request[$key]['TxnID'] = $val->qb_txn_id;
        }
        else {
          // Unsupported operation ( neither add nor mod is enabled.
          break;
        }

        if ($val->qb_name) $request[$key]['Name'] = $val->qb_name;

        $req = array(
          'name' => $name,
          'callback' => array('qb_data_qbwc_response'),
          'data' => array('qb_data' => $row, 'op' => $op, 'value' => $val),
          'request' => $request,
        );

        drupal_alter('qb_data_request', $req);

        $requests[] = $req;
      }
    }
  }

  return $requests;
}

function _qb_data_qbwc_response(&$result, &$data, $status, $message) {
  $query = $data['qb_query'];

  foreach ($result as $i => $r) {
    switch ($data['op']) {
      case 'qry':
        $r['new'] = $r['modified'] = FALSE;
        
        if (!empty($r['ListID'])) {
          $value = db_fetch_object(db_query("SELECT * FROM {qb_data_value}
            WHERE qdid = %d AND qb_list_id = '%s'", $query->qdid, $r['ListID']));
        } 
        else if (!empty($r['TxnID'])) {
          $value = db_fetch_object(db_query("SELECT * FROM {qb_data_value}
            where qdid = %d and qb_txn_id = '%s'", $query->qdid, $r['TxnID']));
        }

        if ($value) {
          $r['modified'] = ($value->qb_edit_sequence != $r['EditSequence']);
        }
        else {
          $r['new'] = TRUE;
          $value = new stdClass();
        }
        break;

      case 'add':
      case 'mod':
        // We'll be getting back an error or the newly-added records.
        $value = $data['value'];
        break;
    }

    // Save the result back in case it was changed.
    $value->qdid = $query->qdid;
    $value->qb_status = (int) ($r['IsActive'] == 'true');
    $value->qb_list_id = $r['ListID'];
    $value->qb_full_name = $r['FullName'];
    $value->qb_name = $r['Name'];
    $value->qb_txn_id = $r['TxnID'];
    $value->qb_txn_number = $r['TxnNumber'];
    $value->qb_edit_sequence = $r['EditSequence'];
    $value->pending = 0;
  
    $key = isset($value->qdvid) ? 'qdvid' : NULL;
    drupal_write_record('qb_data_value', $value, $key);

    $r['value'] = $value;
    $result[$i] = $r;
  }
}

