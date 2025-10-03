#pragma once
#include "Driver.h"

namespace LegoPrinter {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	public ref class MainForm : public System::Windows::Forms::Form
	{
	public:
		MainForm(void)
		{
			InitializeComponent();
		}

	protected:
		~MainForm()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::Button^ button1;
	private: System::Windows::Forms::Button^ button2;
	private: System::Windows::Forms::Button^ button3;
	private: System::Windows::Forms::Button^ button4;

	protected:

	private:
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		void InitializeComponent(void)
		{
			this->button1 = (gcnew System::Windows::Forms::Button());
			this->button2 = (gcnew System::Windows::Forms::Button());
			this->button3 = (gcnew System::Windows::Forms::Button());
			this->button4 = (gcnew System::Windows::Forms::Button());
			this->SuspendLayout();
			// 
			// button1
			// 
			this->button1->Location = System::Drawing::Point(326, 109);
			this->button1->Name = L"button1";
			this->button1->Size = System::Drawing::Size(182, 42);
			this->button1->TabIndex = 0;
			this->button1->Text = L"Connect";
			this->button1->UseVisualStyleBackColor = true;
			this->button1->Click += gcnew System::EventHandler(this, &MainForm::button1_Click);
			// 
			// button2
			// 
			this->button2->Location = System::Drawing::Point(326, 184);
			this->button2->Name = L"button2";
			this->button2->Size = System::Drawing::Size(182, 42);
			this->button2->TabIndex = 1;
			this->button2->Text = L"Test";
			this->button2->UseVisualStyleBackColor = true;
			this->button2->Click += gcnew System::EventHandler(this, &MainForm::button2_Click);
			// 
			// button3
			// 
			this->button3->Location = System::Drawing::Point(326, 267);
			this->button3->Name = L"button3";
			this->button3->Size = System::Drawing::Size(182, 48);
			this->button3->TabIndex = 2;
			this->button3->Text = L"Interpreter";
			this->button3->UseVisualStyleBackColor = true;
			this->button3->Click += gcnew System::EventHandler(this, &MainForm::button3_Click);
			// 
			// button4
			// 
			this->button4->Location = System::Drawing::Point(326, 359);
			this->button4->Name = L"button4";
			this->button4->Size = System::Drawing::Size(182, 48);
			this->button4->TabIndex = 3;
			this->button4->Text = L"Disconnect";
			this->button4->UseVisualStyleBackColor = true;
			this->button4->Click += gcnew System::EventHandler(this, &MainForm::button4_Click);
			// 
			// MainForm
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(8, 16);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(801, 495);
			this->Controls->Add(this->button4);
			this->Controls->Add(this->button3);
			this->Controls->Add(this->button2);
			this->Controls->Add(this->button1);
			this->Name = L"MainForm";
			this->Text = L"MainForm";
			this->ResumeLayout(false);

		}

	private:
		void StartConnect()
		{
			Connect();
		}

#pragma endregion
	private: System::Void button1_Click(System::Object^ sender, System::EventArgs^ e) {
		System::Threading::Thread^ WorkerThread = gcnew System::Threading::Thread(
			gcnew System::Threading::ThreadStart(this, &MainForm::StartConnect)
		);
		WorkerThread->IsBackground = true;
		WorkerThread->Start();
	}
	private: System::Void button2_Click(System::Object^ sender, System::EventArgs^ e) {
		IsTest = true;
	}
	private: System::Void button3_Click(System::Object^ sender, System::EventArgs^ e) {
		IsInterpreter = true;
	}
	private: System::Void button4_Click(System::Object^ sender, System::EventArgs^ e) {
		IsNotDisconnect = false;
	}
};
}
