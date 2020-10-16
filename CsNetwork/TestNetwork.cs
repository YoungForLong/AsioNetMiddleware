using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using CsNetwork;
using System;

public class TestNetwork : MonoBehaviour {

	// Use this for initialization
	void Start () {
        NetworkManager.Instance.Connect("172.0.6.132", 8080, onConnect);

    }

    public Action onConnect = () =>
    {
        Debug.Log("connect succ");
    };

	// Update is called once per frame
	void Update () {
        NetworkManager.Instance.Update(Time.deltaTime);
	}
}
