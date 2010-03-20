<?php // $Id: qb_data.admin.inc,v 1.3 2009/02/17 20:44:29 vauxia Exp $

/**
 * List and manage available queries.
 */
function qb_data_admin_overview() {

  $hdrs = array(
    t('Title'),
    t('Status'),
    t('Frequency'),
    t('Last Run'),
    t('Records'),
    t('Weight'),
    '',
  );

  $rows = array();
  foreach (qb_data_queries(FALSE) as $row) {
    $rows[] = array(
      $row->title,
      $row->status ? '' : t('disabled'),
      $row->frequency,
      $row->lastrun,
      $row->items,
      $row->weight,
      l('edit', 'admin/settings/qb/data/'. $row->qdid),
    );
  }

  $output = '';

  if ($rows) $output .= theme('table', $hdrs, $rows);

  return $output;
}

/**
 * Administer an individual quickbooks query.
 * Form callback for adding/modifying a qb_data query.
 */
function qb_data_admin_form($form_state, $qdid = NULL) {
  $query = ($qdid) ? qb_data_query_load($qdid) : NULL;
  $form = array();
  $form['#data_query'] = $query;

  // Wrapper for the form contents, so that our ajaxy submit kicks in.
  $form['#prefix'] = '<div id="qb-data-admin-form">';
  $form['#suffix'] = '</div>';

  $title = $form_state['storage']['title'];

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#description' => t('A descriptive title for this query to be shown in administrative interfaces'),
    '#default_value' => $title ? $title : $query->title,
    '#required' => TRUE,
  );
 
  $request = $query ? $query->request : $form_state['storage']['request'];

  $form['request'] = array(
    '#type' => 'select',
    '#title' => t('Quickbooks request'),
    '#description' => t('The name of the query that is being used to request information from Quickbooks.'),
    '#options' => qb_data_available_queries(),
    '#default_value' => $request,
  );

  if ($request) {
    $request = current(qb_data_available_queries($request));

    // Let's not go changing this again.
    $form['request']['#disabled'] = TRUE;

    $form['status'] = array(
      '#type' => 'checkbox',
      '#title' => t('Enabled'),
      '#default_value' => isset($query->status) ? $query->status : 1,
      '#description' => t('Include this query in synchronization processes.'),
    );
    $form['options'] = array(
      '#type' => 'checkboxes',
      '#title' => t('Permitted operations'),
      '#description' => t('The operations that are allowed for this query.  If you want your Drupal site to have read-only access to Quickbooks, select only "Query". If you want this site to create or modify Quickbooks data, select those options as well.  <strong>Use with caution!</strong>'),
    );

    foreach (array( 'Qry', 'Add', 'Mod' ) as $action) {
      if (isset($request[strtolower($action)])) {
        $key = 'qb_'. strtolower($action);
        $form['options'][$key] = array(
          '#type' => 'checkbox',
          '#title' => t($action),
          '#default_value' => isset($query->$key) ? $query->$key : 0,
        );
      }
      else {
        $form['options'][$key] = array('#type' => 'value', '#value' => 0);
      }
    }
    $form['options']['qb_qry']['#title'] = t('Query');
    $form['options']['qb_mod']['#title'] = t('Modify');


    $form['frequency'] = array(
      '#type' => 'select',
      '#title' => t('Update frequency'),
      '#default_value' => isset($query->frequency) ? $query->frequency : 3600,
      '#options' => array(
        300  => t('As often as possible'),
        900 => t('Every 15 minutes'),
        3600 => t('Every hour'),
        14400 => t('Every 4 hours'),
        28800 => t('Every 8 hours'),
        43200 => t('Every 12 hours'),
        86400 => t('Once per day'),
      ),
      '#description' => t('If this data changes infrequently, such as service items or employees, you will want to set this value higher to reduce overall load.'),
    );

    $form['min_timestamp'] = array(
      '#type' => 'textfield',
      '#size' => 10,
      '#title' => t('Minimum date'),
      '#description' => t('For things like invoices or time tracking entries, you may not want to import old data into this site.  In that case, set this value to th earliest date of the records you would like to import.'),
    );

    $form['weight'] = array(
      '#type' => 'weight',
      '#title' => t('Weight of data query'),
      '#default_value' => isset($query->weight) ? $query->weight : 0,
      '#description' => t('The weight of the query determines when it is run compared to other queries. The lower the weight the sooner it will run. Usually db_user queries should run before others. This helps when using nodereference fields so that the referenced field exists first.'),
    );
  }
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );

  
  return $form;
}

/**
 * Validation handler for qb_data_admin_form.
 */
function qb_data_admin_form_validate($form, $form_state) {
}

/**
 * Submit handler for qb_data_admin_form.
 */
function qb_data_admin_form_submit($form, &$form_state) {
  if (!$form['#data_query'] && $form_state['values']['request']) {
    if (!count($form_state['storage'])) {
      $form_state['storage'] = $form_state['values'];
      $form_state['rebuild'] = TRUE;
      return;
    }
  }
  qb_data_query_save($form['#data_query'], $form_state['values']);
  unset($form_state['rebuild']);
  $form_state['redirect'] = 'admin/settings/qb/data';
}

/**
 * Save a qb_data record.
 */
function qb_data_query_save($query = NULL, $edit) {
  if ($query && isset($query->request)) {
    // Don't tamper with existing values.
    unset($edit['request'], $edit['lastrun']);
  } 
  else {
    $query = New stdClass();
  }

  foreach ($edit as $key => $val) {
    if (is_array($val)) {
      foreach ($val as $k => $v) $edit[$k] = $v;
      unset($edit[$key]);
    }
    if ($key == 'min_modified') {
      if ($time = strtotime($val)) {
        $edit[$key] = $time;
      }
      else unset($edit[$key]);
    }
  }
  $update = isset($query->qdid) ? 'qdid' : NULL;
  foreach ($edit as $k => $v) $query->$k = $v;
  drupal_write_record('qb_data', $query, $update);
  return $query;
}

/**
 * Delete a qb_data record.
 */
function qb_data_query_delete($query) {
  if ($query->qdid) {
    return db_query('DELETE FROM {qb_data} WHERE qdid = %d', $query->qdid);
  }
  return false;
}

/**
 * Return a query definition.
 */
function qb_data_query_load($qdid) {
  $sql = "SELECT d.*, COUNT(v.qdvid) AS items
    FROM {qb_data} d
    LEFT JOIN {qb_data_value} v USING (qdid)
    WHERE d.qdid = %d GROUP BY d.qdid";

  return db_fetch_object(db_query($sql, $qdid));
}

/**
 * Return all queries that have been created on this site.
 */
function qb_data_queries($active_only = TRUE) {
  $queries = array();

  $sql = "SELECT d.*, COUNT(v.qdvid) AS items
    FROM {qb_data} d
    LEFT JOIN {qb_data_value} v USING (qdid)";
  if ($active_only) $sql .= " WHERE d.status = 1 AND v.status = 1";
  $sql .= " GROUP BY d.qdid ORDER by weight";

  $res = db_query($sql);
  while ($row = db_fetch_object($res)) $queries[$row->qdid] = $row;
  return $queries;
}

/**
 * A list of queries we'd recommend you use, with slightly better names.
 */
function _qb_data_recommended_queries() {
  return array(
    'Customer' =>         t('Customer'),
    'Employee' =>         t('Employee'),
    'Vendor' =>           t('Vendor'),
    'Item' =>             t('Line Item'),
    'ItemDiscount' =>     t('Line Item: Discount'),
    'ItemInventory' =>    t('Line Item: Inventory'),
    'ItemNonInventory' => t('Line Item: Non-inventory'),
    'ItemOtherCharge' =>  t('Line Item: Other Charge'),
    'ItemService' =>      t('Line Item: Service item'),
    'ItemGroup' =>        t('Line Item: Item group'),
    'TimeTracking' =>     t('Time tracking'),
    'Invoice' =>          t('Invoice'),
    'ItemPayment' =>      t('Payment'),
    'ReceivePayment' =>   t('Received Payments'),
    'Journal Entry' =>    t('Journal Entries'),
    'Check' =>            t('Check'),
    'Charge' =>           t('Charge'),
  );
}

/**
 * List all queries that are available from the Quickbooks API
 */
function qb_data_available_queries($group = 'recommended') {
  static $queries;

  if (!$queries) {
    $queries = array();

    // Get all of the available information.
    foreach (qb_qbxml_queries() as $name => $data) {
      $m = array();
      if (preg_match('/(.*)?(Mod|Add|Del|Query)Rq$/', $name, $m)) {
        $category = $m[1];
        if (!isset($queries[$category])) $queries[$category] = array();
        $action = strtolower($m[2]);
        if ($action == 'query') $action = 'qry';
        $queries[$category][$action] = $name;
      }
    }
  }
  
  switch ($group) {
    case 'recommended':
      // Choose from a predefined list of available queries.
      $ret = array();
      foreach (_qb_data_recommended_queries() as $key => $name) {
        if (isset($queries[$key])) $ret[$key] = $name;
      }
      return $ret;

    case 'all':
      // Return everything.
      return array_keys($queries);

    default:
      $ret = array();
      if (!is_array($group)) $group = array($group);
      foreach ($group as $key) {
        if (isset($queries[$key])) $ret[$key] = $queries[$key];
      }
      return $ret;
  }
}
